#pragma once

#include <QObject>
#include <QPointer>
#include <QSystemTrayIcon>

class QAction;
class QMenu;
class QTimer;
class QWindow;

class NativeTray : public QObject {
    Q_OBJECT

public:
    explicit NativeTray(QObject *parent = nullptr);
    ~NativeTray() override;

    Q_INVOKABLE void setWindow(QWindow *window);
    Q_INVOKABLE bool hasWindow() const;
    Q_INVOKABLE void setLanguageFollowDisplay(bool enabled);
    Q_INVOKABLE bool available() const;
    Q_INVOKABLE void showMessage(const QString &title, const QString &message);
    Q_INVOKABLE void showWindow();
    Q_INVOKABLE void hide();
    Q_INVOKABLE void triggerToggleLanguageFollowDisplay();

signals:
    void showWindowRequested();
    void settingsRequested();
    void toggleLanguageFollowDisplayRequested();
    void quitRequested();

private:
    void ensureVisible();
    void updateToggleText();

private:
    QSystemTrayIcon *m_tray = nullptr;
    QMenu *m_menu = nullptr;
    QAction *m_settingsAction = nullptr;
    QAction *m_toggleLanguageFollowAction = nullptr;
    QAction *m_quitAction = nullptr;
    QTimer *m_retryTimer = nullptr;
    QPointer<QWindow> m_window;
    bool m_languageFollowDisplay = true;
    int m_retryCount = 0;
};
