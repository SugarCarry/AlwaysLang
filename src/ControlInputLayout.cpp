#include "ControlInputLayout.h"
#include "GetActiveWindowPath.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <windows.h>
#include <imm.h>

// WM_IME_CONTROL 的子命令常量在部分 Windows SDK 的 imm.h 中未公开定义,
// 这是 Windows 上通用且稳定的值, 手动补充以便跨进程查询 IME 转换模式
#ifndef IMC_GETCONVERSIONMODE
#define IMC_GETCONVERSIONMODE 0x0001
#endif

namespace {
constexpr LANGID ENGLISH_LANG_ID = MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US);
constexpr LANGID CHINESE_LANG_ID = MAKELANGID(LANG_CHINESE, SUBLANG_CHINESE_SIMPLIFIED);
constexpr LANGID KOREAN_LANG_ID = MAKELANGID(LANG_KOREAN, SUBLANG_KOREAN);

QString languageNameFromLangId(LANGID langId) {
    switch (PRIMARYLANGID(langId)) {
        case LANG_ENGLISH:
            return "ENG";
        case LANG_CHINESE:
            return "中";
        case LANG_KOREAN:
            return "한";
        default:
            return "--";
    }
}

bool layoutMatchesLanguage(HKL layout, const QString &language) {
    const auto langId = LOWORD(reinterpret_cast<quintptr>(layout));
    if (language == "ENG") {
        return langId == ENGLISH_LANG_ID;
    }
    if (language == "中") {
        return PRIMARYLANGID(langId) == LANG_CHINESE;
    }
    if (language == "한") {
        return PRIMARYLANGID(langId) == LANG_KOREAN;
    }
    return false;
}
}

ControlInputLayout::ControlInputLayout(QObject *parent)
        : QObject(parent), m_isCapLock(true), m_isTurnOn(false), m_currentLanguage("--"), m_currentTargetLanguage("ENG") {
    m_timer = new QTimer(this);
    m_timer_always = new QTimer(this);

    connect(m_timer, &QTimer::timeout, this, &ControlInputLayout::onTimerTimeout);
    connect(m_timer_always, &QTimer::timeout, this, &ControlInputLayout::onTimerTimeout_always);
}

ControlInputLayout::~ControlInputLayout() {
    stopTask();
    alwaysStoptTask();
}

bool ControlInputLayout::isEnglishInputInstalled() {
    return isInputInstalled("ENG");
}

bool ControlInputLayout::isTargetInputInstalled(const QString &language) {
    return isInputInstalled(language);
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

void ControlInputLayout::gotoLanguageSettings() {
    ShellExecute(NULL, L"open", L"ms-settings:regionlanguage", NULL, NULL, SW_SHOWNORMAL);
}

void ControlInputLayout::startTask() {
    m_timer->start(100);
}

void ControlInputLayout::alwaysStartTask() {
    m_timer_always->start(100);
}

void ControlInputLayout::stopTask() {
    m_timer->stop();
}

void ControlInputLayout::alwaysStoptTask() {
    m_timer_always->stop();
}

void ControlInputLayout::loadCurrentWindowSettings() {
    m_isTurnOn = false;
    m_isCapLock = false;
    m_currentTargetLanguage = "ENG";

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
            setting.targetLanguage = normalizeLanguage(exeInfo.value("targetLanguage").toString("ENG"));
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

    if (m_currentTargetLanguage == "ENG" && m_isCapLock) {
        capLock();
    }
}

void ControlInputLayout::onTimerTimeout_always() {
    switchToLanguage("ENG");
    updateCurrentLanguage();

    if (m_settings->getCapLock()) {
        capLock();
    }
}

void ControlInputLayout::switchToEnglish() {
    switchToLanguage("ENG");
}

void ControlInputLayout::switchToLanguage(const QString &language) {
    const QString normalizedLanguage = normalizeLanguage(language);
    const HKL targetLayout = findInstalledLayout(normalizedLanguage);
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

    if (!layoutMatchesLanguage(currentLayout, normalizedLanguage)) {
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

    const auto langId = LOWORD(reinterpret_cast<quintptr>(hkl));
    const QString layoutLanguage = languageNameFromLangId(langId);

    // 纯英文键盘布局: 直接是英文, 无需再查子模式
    if (layoutLanguage == "ENG" || layoutLanguage == "--") {
        return layoutLanguage;
    }

    // 中文/韩文等 IME: 键盘布局语言无法反映"中/英"子模式
    // 需查询 IME 的转换模式 (IME_CMODE_NATIVE): 置位为本地语言(中/한), 清除为英文
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
    const bool englishMode = !nativeMode || capsOn;

    if (layoutLanguage == "中") {
        return englishMode ? "英" : "中";
    }
    if (layoutLanguage == "한") {
        return englishMode ? "ENG" : "한";
    }
    return layoutLanguage;
}

QString ControlInputLayout::normalizeLanguage(const QString &language) const {
    if (language == "中" || language.compare("ZH", Qt::CaseInsensitive) == 0 ||
        language.compare("CN", Qt::CaseInsensitive) == 0) {
        return "中";
    }
    if (language == "한") {
        return "한";
    }
    return "ENG";
}

bool ControlInputLayout::isInputInstalled(const QString &language) const {
    return findInstalledLayout(language) != nullptr;
}

HKL ControlInputLayout::findInstalledLayout(const QString &language) const {
    HKL layouts[64] = {0};
    int layoutCount = GetKeyboardLayoutList(64, layouts);
    const QString normalizedLanguage = normalizeLanguage(language);

    for (int i = 0; i < layoutCount; i++) {
        if (layoutMatchesLanguage(layouts[i], normalizedLanguage)) {
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

    const HWND foregroundWindow = GetForegroundWindow();
    if (!foregroundWindow) {
        return rect;
    }

    // 优先检查是否为控制台窗口，直接返回固定位置
    wchar_t className[256] = {0};
    if (GetClassNameW(foregroundWindow, className, 256) > 0) {
        if (wcscmp(className, L"ConsoleWindowClass") == 0) {
            // CMD/PowerShell: 显示在窗口左上角偏移位置
            RECT windowRect;
            if (GetWindowRect(foregroundWindow, &windowRect)) {
                POINT point = {windowRect.left + 12, windowRect.top + 80};
                rect["x"] = static_cast<qlonglong>(point.x);
                rect["y"] = static_cast<qlonglong>(point.y);
                rect["width"] = static_cast<qlonglong>(1);
                rect["height"] = static_cast<qlonglong>(24);
                rect["valid"] = true;
                return rect;
            }
        }
    }

    auto setRectFromPoint = [&rect](const POINT &point, LONG width, LONG height) {
        rect["x"] = static_cast<qlonglong>(point.x);
        rect["y"] = static_cast<qlonglong>(point.y);
        rect["width"] = static_cast<qlonglong>(width);
        rect["height"] = static_cast<qlonglong>(height);
        rect["valid"] = true;
    };

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
