#include "NativeTray.h"

#include <QAction>
#include <QApplication>
#include <QIcon>
#include <QMenu>
#include <QStyle>
#include <QTimer>
#include <QWindow>

namespace {
constexpr int kTrayShowRetryIntervalMs = 1000;
constexpr int kMaxTrayShowRetries = 60;
}

NativeTray::NativeTray(QObject *parent)
    : QObject(parent),
      m_tray(new QSystemTrayIcon(this)),
      m_menu(new QMenu()),
      m_retryTimer(new QTimer(this)) {
    auto icon = QIcon(":/res/images/favicon.ico");
    if (icon.isNull()) {
        icon = QApplication::windowIcon();
    }
    if (icon.isNull()) {
        icon = QApplication::style()->standardIcon(QStyle::SP_ComputerIcon);
    }

    m_settingsAction = m_menu->addAction(tr("Settings"));
    connect(m_settingsAction, &QAction::triggered, this, [this]() {
        showWindow();
        emit showWindowRequested();
        emit settingsRequested();
    });

    m_menu->addSeparator();

    m_toggleLanguageFollowAction = m_menu->addAction(QString());
    m_toggleLanguageFollowAction->setCheckable(true);
    connect(m_toggleLanguageFollowAction, &QAction::triggered,
            this, &NativeTray::toggleLanguageFollowDisplayRequested);

    m_menu->addSeparator();

    m_quitAction = m_menu->addAction(tr("Quit"));
    connect(m_quitAction, &QAction::triggered, this, &NativeTray::quitRequested);

    m_tray->setIcon(icon);
    m_tray->setToolTip(QStringLiteral("AlwaysLang"));
    m_tray->setContextMenu(m_menu);
    connect(m_tray, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick) {
            showWindow();
            emit showWindowRequested();
        }
    });

    m_retryTimer->setInterval(kTrayShowRetryIntervalMs);
    connect(m_retryTimer, &QTimer::timeout, this, &NativeTray::ensureVisible);

    updateToggleText();
    ensureVisible();
}

NativeTray::~NativeTray() {
    if (m_retryTimer && m_retryTimer->isActive()) {
        m_retryTimer->stop();
    }

    if (m_tray) {
        m_tray->hide();
        m_tray->setContextMenu(nullptr);
        delete m_tray;
        m_tray = nullptr;
    }

    if (m_menu) {
        delete m_menu;
        m_menu = nullptr;
    }
}

void NativeTray::hide() {
    if (m_retryTimer && m_retryTimer->isActive()) {
        m_retryTimer->stop();
    }

    if (m_tray) {
        m_tray->setContextMenu(nullptr);
        m_tray->hide();
    }
}

void NativeTray::setWindow(QWindow *window) {
    m_window = window;
    ensureVisible();
}

bool NativeTray::hasWindow() const {
    return !m_window.isNull();
}

void NativeTray::setLanguageFollowDisplay(bool enabled) {
    m_languageFollowDisplay = enabled;
    updateToggleText();
}

bool NativeTray::available() const {
    return QSystemTrayIcon::isSystemTrayAvailable();
}

void NativeTray::showWindow() {
    if (!m_window) {
        return;
    }

    if (m_window->visibility() == QWindow::Minimized) {
        m_window->showNormal();
    } else {
        m_window->show();
    }
    m_window->raise();
    m_window->requestActivate();

    QTimer::singleShot(100, this, [this]() {
        if (!m_window) {
            return;
        }
        m_window->show();
        m_window->raise();
        m_window->requestActivate();
    });
}

void NativeTray::ensureVisible() {
    m_tray->show();

    if (QSystemTrayIcon::isSystemTrayAvailable()) {
        if (m_retryTimer->isActive()) {
            m_retryTimer->stop();
        }
        return;
    }

    ++m_retryCount;
    if (m_retryCount < kMaxTrayShowRetries && !m_retryTimer->isActive()) {
        m_retryTimer->start();
    }
}

void NativeTray::updateToggleText() {
    const auto status = m_languageFollowDisplay ? tr("On") : tr("Off");
    m_toggleLanguageFollowAction->setText(tr("Input position language display") + QStringLiteral(": ") + status);
    m_toggleLanguageFollowAction->setChecked(m_languageFollowDisplay);
}

void NativeTray::triggerToggleLanguageFollowDisplay() {
    emit toggleLanguageFollowDisplayRequested();
}
