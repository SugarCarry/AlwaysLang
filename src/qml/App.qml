import QtQuick 2.15
import QtQuick.Window 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import FluentUI 1.0
import "global"

FluLauncher {
    id: app

    // 改变夜间模式时保存设置
    Connections {
        target: FluTheme

        function onDarkModeChanged() {
            SettingsHelper.saveDarkMode(FluTheme.darkMode)
        }
    }

    // 全局快捷键: 开关"光标跟随语言显示"浮窗 (应用不在前台也生效)
    // 仅当用户启用了快捷键功能且组合键有效时才注册
    FluHotkey {
        sequence: GlobalModel.languageFollowHotkeyEnabled ? GlobalModel.languageFollowHotkey : ""
        onActivated: {
            GlobalModel.languageFollowDisplay = !GlobalModel.languageFollowDisplay
            SettingsHelper.saveLanguageFollowDisplay(GlobalModel.languageFollowDisplay)
            NativeTray.setLanguageFollowDisplay(GlobalModel.languageFollowDisplay)

            // 通知 MainWindow 强制刷新浮层
            NativeTray.triggerToggleLanguageFollowDisplay()
        }
    }

    Component.onCompleted: {
        FluApp.init(app)

        FluApp.useSystemAppBar = false

        FluApp.windowIcon = "qrc:/res/images/favicon.ico"

        FluTheme.darkMode = SettingsHelper.getDarkMode()
        GlobalModel.isAlwaysCapLock = SettingsHelper.getCapLock()
        GlobalModel.languageFollowDisplay = SettingsHelper.getLanguageFollowDisplay()
        GlobalModel.languageOverlayOpacity = SettingsHelper.getLanguageOverlayOpacity()
        GlobalModel.languageFollowHotkeyEnabled = SettingsHelper.getLanguageFollowHotkeyEnabled()
        GlobalModel.languageFollowHotkey = SettingsHelper.getLanguageFollowHotkey()
        GlobalModel.currentLanguage = ControlInputLayout.refreshCurrentLanguage()
        GlobalModel.caretRect = ControlInputLayout.caretRect

        // 是否开机自启动
        GlobalModel.isAutoStart = SettingsHelper.getAutoStart();
        GlobalModel.isAutoStartLaunch = Utils.isAutoStartLaunch()
        var openCount = SettingsHelper.getOpenCount()
        openCount += 1
        SettingsHelper.saveOpenCount(openCount)

        try {
            let jsonStr = SettingsHelper.getExistingFilePath();
            const array = JSON.parse(jsonStr);
            const set = new Set(array);
            GlobalModel.existingFilePath = set

            jsonStr = SettingsHelper.getExeInfos()
            const obj = JSON.parse(jsonStr);
            GlobalModel.exeInfos = obj
        } catch (e) {

        }

        FluTheme.animationEnabled = true

        FluRouter.routes = {
            "/": "qrc:/qml/window/MainWindow.qml",
        }
        FluRouter.navigate("/")
    }
}
