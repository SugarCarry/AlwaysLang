pragma Singleton

import QtQuick 2.15
import FluentUI 1.0

QtObject {
    // 开机自启动
    property bool isAutoStart: false
    property bool isAutoStartLaunch: false
    property bool isTaskRunning: false
    property bool hasAutoStartedTask: false

    // 语言跟随显示
    property bool languageFollowDisplay: true
    property string currentLanguage: "--"
    property var caretRect: ({x: 0, y: 0, width: 0, height: 0, valid: false, dark: false})
    // 浮窗不透明度 (10 - 50), 数值越小越透明
    property int languageOverlayOpacity: 30
    // 浮窗背景色: "auto" 为跟随明暗主题, 其余为 "#RRGGBB"
    property string languageOverlayColor: "auto"
    // 开关浮窗跟随显示的全局快捷键
    property bool languageFollowHotkeyEnabled: false
    property string languageFollowHotkey: "Ctrl+Shift+L"

    // 使用 Set 来存储表格的软件路径
    property var existingFilePath: new Set()

    // 使用 Object 来存储表格每项软件的信息 (路径/名称/是否启动/是否开启大小写键)
    property var exeInfos: ({})
}
