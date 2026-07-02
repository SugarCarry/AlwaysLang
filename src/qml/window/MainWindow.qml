import QtQuick 2.15
import QtQuick.Window 2.15
import FluentUI 1.0
import "../global"


FluWindow {

    id: window
    windowIcon: "qrc:/res/images/favicon.ico"
    title: qsTr("AlwaysLang")
    width: 860
    height: 668
    minimumWidth: 668
    minimumHeight: 320

    launchMode: FluWindowType.SingleTask
    autoVisible: !GlobalModel.isAutoStartLaunch
    closeListener: function(event) {
        event.accepted = true
        FluRouter.exit(0)
    }

    appBar: FluAppBar {
        icon: window.windowIcon
        title: window.title
        height: 30
        showDark: true
        darkClickListener: (button) => handleDarkChanged(button)
        closeClickListener: () => {
            minimizeToTray()
        }
        z: 7
    }

    Component.onDestruction: {
        FluRouter.exit()
    }

    Connections {
        target: ControlInputLayout

        function onCurrentLanguageChanged(language) {
            GlobalModel.currentLanguage = language
        }

        function onCaretRectChanged(rect) {
            GlobalModel.caretRect = rect
        }
    }

    FluNavigationView {
        id: nav_view
        width: parent.width
        height: parent.height
        displayMode: FluNavigationViewType.Open
        hideNavAppBar: true
        cellWidth: 150
        z: 999

        pageMode: FluNavigationViewType.Stack

        items: ItemsOriginal
        footerItems: ItemsFooter

        Component.onCompleted: {
            ItemsOriginal.navigationView = nav_view
            ItemsFooter.navigationView = nav_view
            setCurrentIndex(0)
        }
    }

    Connections {
        target: NativeTray

        function onShowWindowRequested() {
            restoreFromTray()
        }

        function onToggleLanguageFollowDisplayRequested() {
            // 强制刷新浮层可见性
            if (GlobalModel.languageFollowDisplay) {
                language_overlay.show()
                language_overlay.visible = true
            } else {
                language_overlay.hide()
                language_overlay.visible = false
            }
        }

        function onSettingsRequested() {
            restoreFromTray()
            nav_view.push("qrc:/qml/page/settings.qml")
        }

        function onQuitRequested() {
            FluRouter.exit(0)
        }
    }

    Timer {
        id: timer_window_hide_delay
        interval: 150
        onTriggered: {
            window.hide()
        }
    }

    Timer {
        // 高频采样 (约 60fps) 让浮窗位置更新更连续, 避免大幅跳变
        // getCaretRect / getCurrentInputLanguage 均为轻量 Win32 调用,
        // 且值未变化时 C++ 端不发信号, 鼠标静止时无额外开销
        interval: 16
        running: GlobalModel.languageFollowDisplay
        repeat: true
        onTriggered: {
            // 触发一次刷新即可, currentLanguage / caretRect 的变化
            // 由上方 Connections 监听 C++ 信号后写入 GlobalModel
            ControlInputLayout.refreshCurrentLanguage()
        }
    }

    Window {
        id: language_overlay
        width: 42
        height: 30

        // 目标位置 (跟随光标 + 偏移)
        readonly property int targetX: GlobalModel.caretRect.valid ? Math.max(0, GlobalModel.caretRect.x + 10) : 0
        readonly property int targetY: GlobalModel.caretRect.valid ? Math.max(0, GlobalModel.caretRect.y + 10) : 0

        x: targetX
        y: targetY
        visible: GlobalModel.languageFollowDisplay && GlobalModel.caretRect.valid
        flags: Qt.Tool | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.WindowDoesNotAcceptFocus | Qt.BypassWindowManagerHint
        color: "transparent"
        // 整体不透明度由设置项控制 (10 - 50 映射为 0.1 - 0.5)
        opacity: Math.max(0.1, Math.min(0.5, GlobalModel.languageOverlayOpacity / 100))

        // 跟随策略: 大距离(快速拖动)直接瞬移, 零延迟跟手;
        // 小距离(细微移动)用快速平滑动画, 去除抖动。
        // Behavior 的 enabled 在位置即将变化时按"目标与当前的距离"判断:
        // 距离 >= 150px 时禁用动画 -> 直接跳到目标, 不再有爬行拖尾
        Behavior on x {
            enabled: GlobalModel.caretRect.valid
                     && Math.abs(language_overlay.targetX - language_overlay.x) < 150
            SmoothedAnimation { velocity: 3000; easing.type: Easing.OutCubic }
        }
        Behavior on y {
            enabled: GlobalModel.caretRect.valid
                     && Math.abs(language_overlay.targetY - language_overlay.y) < 150
            SmoothedAnimation { velocity: 3000; easing.type: Easing.OutCubic }
        }

        // 窗口创建后将其排除出屏幕捕获 (截图/录屏看不到, 本机肉眼可见)
        onVisibleChanged: {
            if (visible) {
                Utils.setExcludeFromCapture(language_overlay, true)
            }
        }

        Rectangle {
            anchors.fill: parent
            radius: 6

            // "auto" 根据浮层背后屏幕画面的明暗自动调整底色, 其余为用户选择的固定颜色
            readonly property bool autoColor: GlobalModel.languageOverlayColor === "auto"
            readonly property bool backdropDark: GlobalModel.caretRect.dark === true
            readonly property color customColor: autoColor ? "transparent" : GlobalModel.languageOverlayColor

            color: autoColor ? (backdropDark ? Qt.rgba(0.12, 0.12, 0.12, 0.9) : Qt.rgba(1, 1, 1, 0.92))
                             : customColor
            border.width: 1
            border.color: autoColor ? (backdropDark ? "#4C4C4C" : "#D6D6D6")
                                    : Qt.darker(customColor, 1.25)

            FluText {
                anchors.centerIn: parent
                text: GlobalModel.currentLanguage
                font: FluTextStyle.BodyStrong
                // 按底色亮度自动选择黑/白文字, 保证可读
                color: {
                    if (parent.autoColor) {
                        return parent.backdropDark ? "#FFFFFF" : "#1A1A1A"
                    }
                    var c = parent.customColor
                    var luminance = 0.299 * c.r + 0.587 * c.g + 0.114 * c.b
                    return luminance > 0.6 ? "#1A1A1A" : "#FFFFFF"
                }
            }
        }
    }

    Component.onCompleted: {
        NativeTray.setWindow(window)
        NativeTray.setLanguageFollowDisplay(GlobalModel.languageFollowDisplay)
    }

    function minimizeToTray() {
        if (NativeTray.available()) {
            timer_window_hide_delay.restart()
        } else {
            restoreFromTray()
        }
    }

    function restoreFromTray() {
        timer_window_hide_delay.stop()
        if (window.visibility === Window.Minimized) {
            window.showNormal()
        } else {
            window.show()
        }
        window.raise()
        window.requestActivate()
    }

    function handleDarkChanged(button) {
        FluTheme.darkMode = FluTheme.dark ? FluThemeType.Light : FluThemeType.Dark
    }
}
