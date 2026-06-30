#include <QApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QTranslator>
#include <QDir>
#include <QObject>
#include <QWindow>
#include <QTimer>
#include "IconProvider.h"
#include "LnkResolver.h"
#include "ControlInputLayout.h"
#include "NativeTray.h"
#include "helper/SettingsHelper.h"
#include "utils.hpp"

int main(int argc, char *argv[]) {
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
#endif
    QApplication app(argc, argv);
    QCoreApplication::setApplicationName("AlwaysLang");
    QApplication::setWindowIcon(QIcon(":/res/images/favicon.ico"));
    QGuiApplication::setQuitOnLastWindowClosed(false);
    QDir::setCurrent(QCoreApplication::applicationDirPath());

    SettingsHelper::getInstance()->init(argv);

    QTranslator translator;
    const QStringList uiLanguages = QLocale::system().uiLanguages();
    for (const QString &locale: uiLanguages) {
        const QString baseName = "AlwaysEnglish_" + QLocale(locale).name();
        if (translator.load(QCoreApplication::applicationDirPath() + "/i18n/" + baseName + ".qm")) {
            app.installTranslator(&translator);
            break;
        }
    }

    NativeTray nativeTray;
    QObject::connect(&nativeTray, &NativeTray::quitRequested, &app, &QApplication::quit);
    QObject::connect(&app, &QCoreApplication::aboutToQuit,
                     &nativeTray, &NativeTray::hide);

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("iconProvider", IconProvider::getInstance());
    engine.rootContext()->setContextProperty("LnkResolver", LnkResolver::getInstance());
    engine.rootContext()->setContextProperty("ControlInputLayout", ControlInputLayout::getInstance());
    engine.rootContext()->setContextProperty("NativeTray", &nativeTray);
    engine.rootContext()->setContextProperty("SettingsHelper", SettingsHelper::getInstance());
    engine.rootContext()->setContextProperty("Utils", Utils::getInstance());

    const QUrl url(QStringLiteral("qrc:/qml/App.qml"));
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
                     &app, [url, &nativeTray](QObject *obj, const QUrl &objUrl) {
                if (!obj && url == objUrl) {
                    QCoreApplication::exit(-1);
                } else if (obj && url == objUrl) {
                    // 兜底: 若 QML 侧未成功调用 setWindow, 这里按标题匹配主窗口
                    // 注意: 不能简单取第一个顶层窗口, 因为语言浮层(language_overlay)
                    // 也是一个顶层 Window, 可能被误选, 导致双击托盘激活的是浮层而非主界面
                    QTimer::singleShot(500, [&nativeTray]() {
                        if (nativeTray.hasWindow()) {
                            return;
                        }
                        const auto windows = qApp->topLevelWindows();
                        QWindow *mainWindow = nullptr;
                        for (auto window : windows) {
                            // 主窗口标题为 AlwaysLang 且尺寸较大, 浮层标题为空且很小
                            if (window && window->title() == QStringLiteral("AlwaysLang")) {
                                mainWindow = window;
                                break;
                            }
                        }
                        // 仍未找到则退而取尺寸最大的窗口
                        if (!mainWindow) {
                            for (auto window : windows) {
                                if (!window) {
                                    continue;
                                }
                                if (!mainWindow || window->width() > mainWindow->width()) {
                                    mainWindow = window;
                                }
                            }
                        }
                        if (mainWindow) {
                            nativeTray.setWindow(mainWindow);
                        }
                    });
                }
            }, Qt::QueuedConnection);
    engine.load(url);

    return app.exec();
}
