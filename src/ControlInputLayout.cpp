#include "ControlInputLayout.h"
#include "GetActiveWindowPath.h"
#include <QElapsedTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <windows.h>
#include <imm.h>

// WM_IME_CONTROL 的子命令常量在部分 Windows SDK 的 imm.h 中未公开定义,
// 这是 Windows 上通用且稳定的值, 手动补充以便跨进程查询 IME 转换模式
#ifndef IMC_GETCONVERSIONMODE
#define IMC_GETCONVERSIONMODE 0x0001
#endif

namespace {
constexpr int ENGLISH_US_LANG_ID = 0x0409;

LANGID langIdFromLayout(HKL layout) {
    return LOWORD(reinterpret_cast<quintptr>(layout));
}

QString localeNameFromLangId(LANGID langId) {
    wchar_t locale[LOCALE_NAME_MAX_LENGTH] = {0};
    if (!LCIDToLocaleName(MAKELCID(langId, SORT_DEFAULT), locale, LOCALE_NAME_MAX_LENGTH, 0)) {
        return QString();
    }
    return QString::fromWCharArray(locale);
}

// 浮层/下拉框用的短标签: 常用语言用习惯写法, 其余用 ISO 语言代码大写
QString shortLabelForLangId(LANGID langId) {
    switch (PRIMARYLANGID(langId)) {
        case LANG_ENGLISH:
            return "ENG";
        case LANG_CHINESE:
            return "中";
        case LANG_KOREAN:
            return "한";
        case LANG_JAPANESE:
            return "日";
        default: {
            const QString locale = localeNameFromLangId(langId);
            if (locale.isEmpty()) {
                return "--";
            }
            return locale.section('-', 0, 0).toUpper();
        }
    }
}

// 系统 UI 语言下的本地化语言显示名, 如 "中文(中华人民共和国)"
QString displayNameForLangId(LANGID langId) {
    const QString locale = localeNameFromLangId(langId);
    if (locale.isEmpty()) {
        return QString();
    }

    wchar_t display[256] = {0};
    if (GetLocaleInfoEx(reinterpret_cast<LPCWSTR>(locale.utf16()),
                        LOCALE_SLOCALIZEDDISPLAYNAME, display, 256) > 0) {
        return QString::fromWCharArray(display);
    }
    return locale;
}

// 读取控制台光标位置需短暂附加到目标控制台, 期间若用户按下
// Ctrl+C / Ctrl+Break, 事件会发给所有附加进程, 一律忽略以免本程序被终止
BOOL WINAPI ignoreConsoleCtrlEvents(DWORD /*ctrlType*/) {
    return TRUE;
}

// 读取前台控制台窗口的光标屏幕位置与字符格尺寸
// 控制台窗口的 GetWindowThreadProcessId 返回的是控制台程序(如 cmd)的 PID,
// 附加到它的控制台后即可读取输出缓冲区的光标字符坐标
bool queryConsoleCaret(HWND consoleWindow, POINT &point, LONG &cellWidth, LONG &cellHeight) {
    DWORD processId = 0;
    GetWindowThreadProcessId(consoleWindow, &processId);
    if (!processId || !AttachConsole(processId)) {
        return false;
    }

    bool ok = false;
    const HANDLE conout = CreateFileW(L"CONOUT$", GENERIC_READ | GENERIC_WRITE,
                                      FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                                      OPEN_EXISTING, 0, NULL);
    if (conout != INVALID_HANDLE_VALUE) {
        CONSOLE_SCREEN_BUFFER_INFO bufferInfo = {};
        CONSOLE_FONT_INFO fontInfo = {};
        if (GetConsoleScreenBufferInfo(conout, &bufferInfo)
            && GetCurrentConsoleFont(conout, FALSE, &fontInfo)
            && fontInfo.dwFontSize.X > 0 && fontInfo.dwFontSize.Y > 0
            // 光标滚出可视区域(纵向或横向)时不显示, 避免浮层定位到窗外
            && bufferInfo.dwCursorPosition.Y >= bufferInfo.srWindow.Top
            && bufferInfo.dwCursorPosition.Y <= bufferInfo.srWindow.Bottom
            && bufferInfo.dwCursorPosition.X >= bufferInfo.srWindow.Left
            && bufferInfo.dwCursorPosition.X <= bufferInfo.srWindow.Right) {
            POINT origin = {0, 0};
            if (ClientToScreen(consoleWindow, &origin)) {
                cellWidth = fontInfo.dwFontSize.X;
                cellHeight = fontInfo.dwFontSize.Y;
                point.x = origin.x + (bufferInfo.dwCursorPosition.X - bufferInfo.srWindow.Left) * cellWidth;
                // 取光标所在行的底部, 与 GUI 窗口的插入符定位一致
                point.y = origin.y + (bufferInfo.dwCursorPosition.Y - bufferInfo.srWindow.Top + 1) * cellHeight;
                ok = true;
            }
        }
        CloseHandle(conout);
    }
    FreeConsole();
    return ok;
}

// 采样光标附近屏幕像素的平均亮度, 判断浮层背后的背景是深色还是浅色,
// 供浮层"自动"配色使用 (深色背景配深底白字, 浅色背景配浅底黑字)
// GetPixel 较慢, 限频复用; 阈值带滞回, 避免亮度在临界值附近时来回闪烁
bool isBackdropDark(const POINT &caretPoint) {
    static QElapsedTimer throttle;
    static bool cachedDark = false;

    if (throttle.isValid() && throttle.elapsed() < 150) {
        return cachedDark;
    }
    throttle.restart();

    const HDC screenDc = GetDC(NULL);
    if (!screenDc) {
        return cachedDark;
    }

    // 在光标左侧与上方取样: 这是正在输入的文本区域背景,
    // 且避开浮层自身 (浮层显示在光标右下方, 且已排除出屏幕捕获)
    const POINT samples[] = {
            {caretPoint.x - 8,  caretPoint.y - 8},
            {caretPoint.x - 24, caretPoint.y - 14},
            {caretPoint.x + 4,  caretPoint.y - 24},
    };

    double total = 0;
    int counted = 0;
    for (const POINT &sample: samples) {
        const COLORREF color = GetPixel(screenDc, sample.x, sample.y);
        if (color == CLR_INVALID) {
            continue;
        }
        total += 0.299 * GetRValue(color) + 0.587 * GetGValue(color) + 0.114 * GetBValue(color);
        counted++;
    }
    ReleaseDC(NULL, screenDc);

    if (counted == 0) {
        return cachedDark;
    }

    const double luminance = total / counted / 255.0;
    if (cachedDark && luminance > 0.55) {
        cachedDark = false;
    } else if (!cachedDark && luminance < 0.45) {
        cachedDark = true;
    }
    return cachedDark;
}
}

ControlInputLayout::ControlInputLayout(QObject *parent)
        : QObject(parent), m_isCapLock(true), m_isTurnOn(false), m_currentLanguage("--"),
          m_currentTargetLanguage(ENGLISH_US_LANG_ID) {
    m_timer = new QTimer(this);

    connect(m_timer, &QTimer::timeout, this, &ControlInputLayout::onTimerTimeout);

    // 读取控制台光标位置需短暂附加到目标控制台,
    // 注册忽略处理器, 防止期间的 Ctrl+C / Ctrl+Break 事件终止本程序
    SetConsoleCtrlHandler(ignoreConsoleCtrlEvents, TRUE);
}

ControlInputLayout::~ControlInputLayout() {
    stopTask();
}

QVariantList ControlInputLayout::installedLanguages() const {
    QVariantList result;
    QSet<int> seenLangIds;
    QHash<QString, int> labelCount;

    HKL layouts[64] = {0};
    const int layoutCount = GetKeyboardLayoutList(64, layouts);

    for (int i = 0; i < layoutCount; i++) {
        const LANGID langId = langIdFromLayout(layouts[i]);
        if (seenLangIds.contains(langId)) {
            continue;
        }
        seenLangIds.insert(langId);

        // Win10/11 部分新增语言使用瞬态 LANGID (0x1000), 查不到区域名,
        // 无法生成有效标签且多个此类语言无法互相区分, 跳过
        if (localeNameFromLangId(langId).isEmpty()) {
            continue;
        }

        QVariantMap language;
        language["langId"] = static_cast<int>(langId);
        language["label"] = shortLabelForLangId(langId);
        language["name"] = displayNameForLangId(langId);
        result.append(language);

        labelCount[language["label"].toString()]++;
    }

    // 同一语言的多地区布局 (如 en-US / en-GB) 短标签相同, 改用完整区域代码区分
    for (auto &item: result) {
        QVariantMap language = item.toMap();
        if (labelCount.value(language["label"].toString()) > 1) {
            language["label"] = localeNameFromLangId(static_cast<LANGID>(language["langId"].toInt())).toUpper();
            item = language;
        }
    }

    return result;
}

bool ControlInputLayout::isTargetInputInstalled(int langId) {
    return findInstalledLayout(langId) != nullptr;
}

int ControlInputLayout::legacyLanguageToId(const QString &language) const {
    if (language == "中" || language.compare("ZH", Qt::CaseInsensitive) == 0 ||
        language.compare("CN", Qt::CaseInsensitive) == 0) {
        return 0x0804;  // zh-CN
    }
    if (language == "한" || language.compare("KO", Qt::CaseInsensitive) == 0) {
        return 0x0412;  // ko-KR
    }
    return ENGLISH_US_LANG_ID;
}

QString ControlInputLayout::currentLanguage() const {
    return m_currentLanguage;
}

QVariantMap ControlInputLayout::caretRect() const {
    return m_caretRect;
}

QString ControlInputLayout::refreshCurrentLanguage() {
    updateCurrentLanguage();
    updateCaretRect();
    return m_currentLanguage;
}

void ControlInputLayout::startTask() {
    m_timer->start(100);
}

void ControlInputLayout::stopTask() {
    m_timer->stop();
}

void ControlInputLayout::loadCurrentWindowSettings() {
    m_isTurnOn = false;
    m_isCapLock = false;
    m_currentTargetLanguage = ENGLISH_US_LANG_ID;

    // 配置只在 UI 保存时变化, 原始字符串未变时复用上次的解析结果,
    // 避免 100ms 轮询每个 tick 都全量解析 JSON
    const QString raw = m_settings->getExeInfos();
    if (raw != m_exeInfosCacheRaw) {
        m_exeInfosCacheRaw = raw;
        m_exeInfosCache.clear();

        const QJsonObject infos = QJsonDocument::fromJson(raw.toUtf8()).object();
        for (auto it = infos.begin(); it != infos.end(); ++it) {
            const QString name = GetActiveWindowPath::extractExeName(it.key()).toLower();
            if (name.isEmpty()) {
                continue;
            }
            const QJsonObject exeInfo = it.value().toObject();
            AppSetting setting;
            setting.isTurnOn = exeInfo.value("isTurnOn").toBool();
            setting.isCapLock = exeInfo.value("isCapLock").toBool();

            // 新版本存 LANGID 数值, 旧版本存 ENG/中/한 字符串
            const QJsonValue langValue = exeInfo.value("targetLanguage");
            setting.targetLanguage = langValue.isString()
                                     ? legacyLanguageToId(langValue.toString())
                                     : langValue.toInt(ENGLISH_US_LANG_ID);
            m_exeInfosCache.insert(name, setting);
        }
    }

    // 按 exe 名称精确匹配, 避免子串误匹配 (如 cmd 误匹配 cmder)
    const auto it = m_exeInfosCache.constFind(gw->exeName.toLower());
    if (it != m_exeInfosCache.constEnd()) {
        m_isTurnOn = it->isTurnOn;
        m_isCapLock = it->isCapLock;
        m_currentTargetLanguage = it->targetLanguage;
    }
}

void ControlInputLayout::onTimerTimeout() {
    updateCurrentLanguage();

    if (!gw->isTargetWindow()) {
        return;
    }

    loadCurrentWindowSettings();

    if (!m_isTurnOn) {
        return;
    }

    switchToLanguage(m_currentTargetLanguage);
    updateCurrentLanguage();

    if (PRIMARYLANGID(static_cast<LANGID>(m_currentTargetLanguage)) == LANG_ENGLISH && m_isCapLock) {
        capLock();
    }
}

void ControlInputLayout::switchToLanguage(int langId) {
    const HKL targetLayout = findInstalledLayout(langId);
    if (!targetLayout) {
        return;
    }

    const HWND foregroundWindow = GetForegroundWindow();
    if (!foregroundWindow) {
        return;
    }

    HKL currentLayout = keyboardLayoutForWindow(foregroundWindow);
    if (!currentLayout) {
        return;
    }

    // 已是目标主语言时不再重发切换请求: 既避免每 100ms 重复触发,
    // 也不会把用户在同语言多地区布局间的手动选择 (如 zh-CN/zh-TW) 强行翻回
    if (PRIMARYLANGID(langIdFromLayout(currentLayout)) != PRIMARYLANGID(langIdFromLayout(targetLayout))) {
        PostMessage(foregroundWindow, WM_INPUTLANGCHANGEREQUEST, 0,
                    reinterpret_cast<LPARAM>(targetLayout));
    }
}

void ControlInputLayout::capLock() {
    // 检查大小写键是否按下, 如果没有则模拟按下并释放大小写键
    if (!(GetKeyState(VK_CAPITAL) & 0x0001)) {
        keybd_event(VK_CAPITAL, 0x3a, KEYEVENTF_EXTENDEDKEY | 0, 0);
        keybd_event(VK_CAPITAL, 0x3a, KEYEVENTF_EXTENDEDKEY | KEYEVENTF_KEYUP, 0);
    }
}

HKL ControlInputLayout::keyboardLayoutForWindow(HWND window) const {
    if (!window) {
        return nullptr;
    }

    const DWORD threadId = GetWindowThreadProcessId(window, NULL);
    if (const HKL layout = GetKeyboardLayout(threadId)) {
        return layout;
    }

    // 控制台窗口(CMD/PowerShell)由 conhost 托管, GetWindowThreadProcessId
    // 返回的是控制台程序的伪线程 ID, 用它查键盘布局得到 0;
    // 而其默认 IME 窗口属于 conhost 的真实输入线程, 经该线程可查到布局
    const HWND imeWindow = ImmGetDefaultIMEWnd(window);
    if (!imeWindow) {
        return nullptr;
    }
    const DWORD imeThreadId = GetWindowThreadProcessId(imeWindow, NULL);
    if (!imeThreadId) {
        // 传 0 给 GetKeyboardLayout 查到的是自己线程的布局, 不能作为回退
        return nullptr;
    }
    return GetKeyboardLayout(imeThreadId);
}

QString ControlInputLayout::getCurrentInputLanguage() const {
    const HWND foregroundWindow = GetForegroundWindow();
    if (!foregroundWindow) {
        return "--";
    }

    HKL hkl = keyboardLayoutForWindow(foregroundWindow);
    if (!hkl) {
        return "--";
    }

    const LANGID langId = langIdFromLayout(hkl);
    const QString layoutLabel = shortLabelForLangId(langId);
    const WORD primary = PRIMARYLANGID(langId);

    // 仅中日韩输入法有"本地语言/英文"子模式, 其余布局直接显示语言标签
    if (primary != LANG_CHINESE && primary != LANG_JAPANESE && primary != LANG_KOREAN) {
        return layoutLabel;
    }

    // 需查询 IME 的转换模式 (IME_CMODE_NATIVE): 置位为本地语言, 清除为英文
    // 跨进程查询用 WM_IME_CONTROL + IMC_GETCONVERSIONMODE 发送到目标窗口的默认 IME 窗口
    // (控制台窗口的默认 IME 窗口属于 conhost, 同样能响应该查询)
    bool nativeMode = false;  // 默认为英文模式, 避免查询失败时误判
    const HWND hwndIme = ImmGetDefaultIMEWnd(foregroundWindow);

    if (hwndIme) {
        DWORD_PTR conversionMode = 0;
        const LRESULT ok = SendMessageTimeoutW(hwndIme, WM_IME_CONTROL,
                                               IMC_GETCONVERSIONMODE, 0,
                                               SMTO_ABORTIFHUNG | SMTO_BLOCK, 30,
                                               &conversionMode);
        if (ok) {
            nativeMode = (conversionMode & IME_CMODE_NATIVE) != 0;
        }
    }

    // Caps Lock 打开时, 中文 IME 实际输入的是英文字母
    const bool capsOn = (GetKeyState(VK_CAPITAL) & 0x0001) != 0;
    if (nativeMode && !capsOn) {
        return layoutLabel;
    }
    return primary == LANG_CHINESE ? "英" : "ENG";
}

HKL ControlInputLayout::findInstalledLayout(int langId) const {
    HKL layouts[64] = {0};
    const int layoutCount = GetKeyboardLayoutList(64, layouts);
    const LANGID target = static_cast<LANGID>(langId);

    // 先精确匹配完整 LANGID (含地区)
    for (int i = 0; i < layoutCount; i++) {
        if (langIdFromLayout(layouts[i]) == target) {
            return layouts[i];
        }
    }

    // 再退回只匹配主语言 (如设置存的 zh-CN 而系统装的是 zh-TW)
    for (int i = 0; i < layoutCount; i++) {
        if (PRIMARYLANGID(langIdFromLayout(layouts[i])) == PRIMARYLANGID(target)) {
            return layouts[i];
        }
    }

    return nullptr;
}

void ControlInputLayout::updateCurrentLanguage() {
    const QString language = getCurrentInputLanguage();
    if (language == m_currentLanguage) {
        return;
    }

    m_currentLanguage = language;
    emit currentLanguageChanged(m_currentLanguage);
}

QVariantMap ControlInputLayout::getCaretRect() const {
    QVariantMap rect;
    rect["x"] = 0;
    rect["y"] = 0;
    rect["width"] = 0;
    rect["height"] = 0;
    rect["valid"] = false;
    rect["dark"] = false;

    const HWND foregroundWindow = GetForegroundWindow();
    if (!foregroundWindow) {
        return rect;
    }

    auto setRectFromPoint = [&rect](const POINT &point, LONG width, LONG height) {
        rect["x"] = static_cast<qlonglong>(point.x);
        rect["y"] = static_cast<qlonglong>(point.y);
        rect["width"] = static_cast<qlonglong>(width);
        rect["height"] = static_cast<qlonglong>(height);
        rect["valid"] = true;
        // 浮层"自动"配色依据: 光标附近屏幕背景是深色还是浅色
        rect["dark"] = isBackdropDark(point);
    };

    // 控制台窗口: 插入符由 conhost 内部绘制, GetGUIThreadInfo 拿不到,
    // 需短暂附加到目标控制台读取光标的字符坐标再换算为屏幕像素
    wchar_t className[256] = {0};
    if (GetClassNameW(foregroundWindow, className, 256) > 0
        && wcscmp(className, L"ConsoleWindowClass") == 0) {
        // 附加/分离有开销, 且附加期间存在极小的控制台事件窗口, 限频复用上次结果
        static QElapsedTimer throttle;
        static HWND cachedWindow = nullptr;
        static POINT cachedPoint = {};
        static LONG cachedCellW = 0;
        static LONG cachedCellH = 0;
        static bool cachedValid = false;

        if (!throttle.isValid() || throttle.elapsed() >= 50 || cachedWindow != foregroundWindow) {
            cachedValid = queryConsoleCaret(foregroundWindow, cachedPoint, cachedCellW, cachedCellH);
            cachedWindow = foregroundWindow;
            throttle.restart();
        }

        if (cachedValid) {
            setRectFromPoint(cachedPoint, cachedCellW, cachedCellH);
            return rect;
        }

        // 读取失败 (如管理员权限的控制台): 退回窗口左上角固定偏移
        RECT windowRect;
        if (GetWindowRect(foregroundWindow, &windowRect)) {
            POINT point = {windowRect.left + 12, windowRect.top + 80};
            setRectFromPoint(point, 1, 24);
        }
        return rect;
    }

    const DWORD threadId = GetWindowThreadProcessId(foregroundWindow, NULL);
    auto updateFromGuiThread = [&](DWORD idThread) {
        GUITHREADINFO guiThreadInfo = {0};
        guiThreadInfo.cbSize = sizeof(GUITHREADINFO);
        if (!GetGUIThreadInfo(idThread, &guiThreadInfo) || !guiThreadInfo.hwndCaret) {
            return false;
        }

        POINT point = {guiThreadInfo.rcCaret.left, guiThreadInfo.rcCaret.bottom};
        ClientToScreen(guiThreadInfo.hwndCaret, &point);
        setRectFromPoint(point,
                         guiThreadInfo.rcCaret.right - guiThreadInfo.rcCaret.left,
                         guiThreadInfo.rcCaret.bottom - guiThreadInfo.rcCaret.top);
        return true;
    };

    if (updateFromGuiThread(threadId) || updateFromGuiThread(0)) {
        return rect;
    }

    HIMC inputContext = ImmGetContext(foregroundWindow);
    if (inputContext) {
        COMPOSITIONFORM compositionForm;
        if (ImmGetCompositionWindow(inputContext, &compositionForm)) {
            POINT point = compositionForm.ptCurrentPos;
            ClientToScreen(foregroundWindow, &point);
            ImmReleaseContext(foregroundWindow, inputContext);

            setRectFromPoint(point, 1, 24);
            return rect;
        }
        ImmReleaseContext(foregroundWindow, inputContext);
    }

    POINT cursorPoint;
    if (GetCursorPos(&cursorPoint)) {
        setRectFromPoint(cursorPoint, 1, 24);
        return rect;
    }

    RECT windowRect;
    if (GetWindowRect(foregroundWindow, &windowRect)) {
        // 其他窗口: 使用默认偏移
        POINT point = {windowRect.left + 16, windowRect.top + 48};
        setRectFromPoint(point, 1, 24);
    }
    return rect;
}

void ControlInputLayout::updateCaretRect() {
    const QVariantMap rect = getCaretRect();
    if (rect == m_caretRect) {
        return;
    }

    m_caretRect = rect;
    emit caretRectChanged(m_caretRect);
}
