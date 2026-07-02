#pragma once

#include <QObject>
#include <QFileInfo>
#include <QSet>
#include <Windows.h>
#include "singleton.h"
#include "helper/SettingsHelper.h"

class GetActiveWindowPath : public QObject {
Q_OBJECT

private:
    explicit GetActiveWindowPath(QObject *parent = nullptr);

    QString GetProcessPathByWindowHandle(HWND hwnd);

    QString getCurrentActiveWindow();

public:
SINGLETON(GetActiveWindowPath)

    // 从路径中提取不带扩展名的 exe 名称, 提取失败返回空串
    static QString extractExeName(const QString &path);

    bool isTargetWindow();

public:
    QString exeName;

private:
    SettingsHelper *m_settings = SettingsHelper::getInstance();

    QString m_existingCacheRaw;
    QSet<QString> m_existingExeNames;
};
