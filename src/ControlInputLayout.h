#pragma once

#include <QHash>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>
#include <windows.h>
#include "singleton.h"
#include "helper/SettingsHelper.h"
#include "GetActiveWindowPath.h"

class ControlInputLayout : public QObject {
Q_OBJECT
    Q_PROPERTY(QString currentLanguage READ currentLanguage NOTIFY currentLanguageChanged)
    Q_PROPERTY(QVariantMap caretRect READ caretRect NOTIFY caretRectChanged)

private:
    // 单个软件的跟随配置 (解析自 exeInfos, 以小写 exe 名为键缓存)
    struct AppSetting {
        bool isTurnOn = false;
        bool isCapLock = false;
        int targetLanguage = 0x0409;  // LANGID, 默认 en-US
    };

    bool m_isCapLock;
    bool m_isTurnOn;
    QString m_currentLanguage;
    int m_currentTargetLanguage;  // LANGID

    QVariantMap m_caretRect;

    QString m_exeInfosCacheRaw;
    QHash<QString, AppSetting> m_exeInfosCache;

    QTimer *m_timer;

    SettingsHelper *m_settings = SettingsHelper::getInstance();

    GetActiveWindowPath *gw = GetActiveWindowPath::getInstance();

private:
    explicit ControlInputLayout(QObject *parent = nullptr);

    ~ControlInputLayout();

    void switchToLanguage(int langId);

    void capLock();

    void loadCurrentWindowSettings();

    QString getCurrentInputLanguage() const;

    HKL keyboardLayoutForWindow(HWND window) const;

    QVariantMap getCaretRect() const;

    HKL findInstalledLayout(int langId) const;

    void updateCurrentLanguage();

    void updateCaretRect();

public:
SINGLETON(ControlInputLayout)

    // 枚举系统已安装的输入语言 (按 LANGID 去重)
    // 每项: { langId: int, label: 短标签(如 ENG/中/한/日/FR), name: 本地化语言名 }
    Q_INVOKABLE QVariantList installedLanguages() const;

    Q_INVOKABLE bool isTargetInputInstalled(int langId);

    // 旧版本以字符串 (ENG / 中 / 한 / ZH / CN) 保存目标语言, 转换为 LANGID
    Q_INVOKABLE int legacyLanguageToId(const QString &language) const;

    Q_INVOKABLE QString currentLanguage() const;

    Q_INVOKABLE QVariantMap caretRect() const;

    Q_INVOKABLE QString refreshCurrentLanguage();

    Q_INVOKABLE void startTask();

    Q_INVOKABLE void stopTask();

signals:
    void currentLanguageChanged(const QString &language);

    void caretRectChanged(const QVariantMap &rect);

private slots:

    void onTimerTimeout();
};
