#include "GetActiveWindowPath.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>

GetActiveWindowPath::GetActiveWindowPath(QObject *parent) : QObject(parent) {}

QString GetActiveWindowPath::GetProcessPathByWindowHandle(HWND hwnd) {
    DWORD processId = 0;
    if (GetWindowThreadProcessId(hwnd, &processId) == 0 || processId == 0) {
        return QString();
    }

    // PROCESS_QUERY_LIMITED_INFORMATION 无需高权限,
    // 普通权限运行时也能取到"以管理员身份运行"的前台窗口的进程路径
    HANDLE processHandle = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId);
    if (processHandle == NULL) {
        return QString();
    }

    WCHAR processPath[MAX_PATH];
    DWORD pathLength = MAX_PATH;
    const BOOL ok = QueryFullProcessImageNameW(processHandle, 0, processPath, &pathLength);
    CloseHandle(processHandle);

    if (!ok) {
        return QString();
    }

    QFileInfo fileInfo(QString::fromWCharArray(processPath, pathLength));

    return fileInfo.filePath();
}

QString GetActiveWindowPath::getCurrentActiveWindow() {
    HWND foreGroundWindowHwnd = GetForegroundWindow();

    if (NULL == foreGroundWindowHwnd || !IsWindow(foreGroundWindowHwnd)) {
        return QString();
    }

    return GetProcessPathByWindowHandle(foreGroundWindowHwnd);
}

QString GetActiveWindowPath::extractExeName(const QString &path) {
    static const QRegularExpression pattern(R"(([^/\\]+)\.exe$)",
                                            QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatch match = pattern.match(path);

    if (match.hasMatch()) {
        return match.captured(1);
    }
    return "";
}

bool GetActiveWindowPath::isTargetWindow() {
    // 获取当前正在使用的窗口的软件所在路径
    const QString exePath = getCurrentActiveWindow();

    // 提取软件的名称
    exeName = extractExeName(exePath);
    if (exeName.isEmpty()) {
        return false;
    }

    // 软件列表以 JSON 数组字符串保存, 只在 UI 保存时变化,
    // 原始字符串未变时复用上次解析出的 exe 名集合
    const QString raw = m_settings->getExistingFilePath();
    if (raw != m_existingCacheRaw) {
        m_existingCacheRaw = raw;
        m_existingExeNames.clear();

        const QJsonArray fileList = QJsonDocument::fromJson(raw.toUtf8()).array();
        for (const auto &item: fileList) {
            const QString name = extractExeName(item.toString()).toLower();
            if (!name.isEmpty()) {
                m_existingExeNames.insert(name);
            }
        }
    }

    // 按 exe 名称精确匹配, 避免子串误匹配 (如 cmd 误匹配 cmder)
    return m_existingExeNames.contains(exeName.toLower());
}
