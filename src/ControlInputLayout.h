#pragma once

#include <QHash>
#include <QObject>
#include <QPoint>
#include <QString>
#include <QTimer>
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
        QString targetLanguage = QStringLiteral("ENG");
    };

    bool m_isCapLock;
    bool m_isTurnOn;
    QString m_currentLanguage;
    QString m_currentTargetLanguage;
    QVariantMap m_caretRect;

    QString m_exeInfosCacheRaw;
    QHash<QString, AppSetting> m_exeInfosCache;

    QTimer *m_timer;
    QTimer *m_timer_always;

    SettingsHelper *m_settings = SettingsHelper::getInstance();

    GetActiveWindowPath *gw = GetActiveWindowPath::getInstance();

private:
    explicit ControlInputLayout(QObject *parent = nullptr);

    ~ControlInputLayout();

    void switchToLanguage(const QString &language);

    void switchToEnglish();

    void capLock();

    void loadCurrentWindowSettings();

    QString getCurrentInputLanguage() const;

    HKL keyboardLayoutForWindow(HWND window) const;

    QVariantMap getCaretRect() const;

    QString normalizeLanguage(const QString &language) const;

    bool isInputInstalled(const QString &language) const;

    HKL findInstalledLayout(const QString &language) const;

    void updateCurrentLanguage();

    void updateCaretRect();

public:
SINGLETON(ControlInputLayout)

    Q_INVOKABLE bool isEnglishInputInstalled();

    Q_INVOKABLE bool isTargetInputInstalled(const QString &language);

    Q_INVOKABLE QString currentLanguage() const;

    Q_INVOKABLE QVariantMap caretRect() const;

    Q_INVOKABLE QString refreshCurrentLanguage();

    Q_INVOKABLE void gotoLanguageSettings();

    Q_INVOKABLE void startTask();

    Q_INVOKABLE void alwaysStartTask();

    Q_INVOKABLE void stopTask();

    Q_INVOKABLE void alwaysStoptTask();

signals:
    void currentLanguageChanged(const QString &language);

    void caretRectChanged(const QVariantMap &rect);

private slots:

    void onTimerTimeout();

    void onTimerTimeout_always();
};
