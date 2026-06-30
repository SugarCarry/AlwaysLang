import QtQuick 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15
import QtQuick.Controls 2.15
import FluentUI 1.0
import "../global"

FluScrollablePage {
    title: qsTr("Settings")

    FluFrame {
        Layout.fillWidth: true
        Layout.topMargin: 20
        height: 128
        padding: 10

        ColumnLayout {
            spacing: 5
            anchors {
                top: parent.top
                left: parent.left
            }
            FluText {
                text: qsTr("Dark Mode")
                font: FluTextStyle.BodyStrong
                Layout.bottomMargin: 4
            }
            Repeater {
                model: [{title: qsTr("System"), mode: FluThemeType.System}, {
                    title: qsTr("Light"),
                    mode: FluThemeType.Light
                }, {title: qsTr("Dark"), mode: FluThemeType.Dark}]
                delegate: FluRadioButton {
                    checked: FluTheme.darkMode === modelData.mode
                    text: modelData.title
                    clickListener: function () {
                        FluTheme.darkMode = modelData.mode
                    }
                }
            }
        }
    }

    FluFrame {
        Layout.fillWidth: true
        Layout.topMargin: 20
        height: 50
        padding: 10

        FluCheckBox {
            id: _autoStart

            text: qsTr("Running at start")
            anchors.verticalCenter: parent.verticalCenter

            onClicked: {
                if (!Utils.setAutoStart(checked)) {
                    checked = !checked
                    showError(qsTr("Please run AlwaysLang as administrator once to change startup settings"), 5000)
                    return
                }

                GlobalModel.isAutoStart = checked
                SettingsHelper.saveAutoStart(GlobalModel.isAutoStart)
            }

            Component.onCompleted: {
                _autoStart.checked = GlobalModel.isAutoStart
            }
        }
    }

    FluFrame {
        Layout.fillWidth: true
        Layout.topMargin: 20
        // 高度随内容自适应 (含复选框/透明度/快捷键三行), 避免固定高度裁切
        height: _languageFollowColumn.implicitHeight + 20
        padding: 10

        ColumnLayout {
            id: _languageFollowColumn
            anchors.fill: parent
            spacing: 8

            FluCheckBox {
                id: _languageFollowDisplay
                text: qsTr("Show current input language near cursor")

                onClicked: {
                    GlobalModel.languageFollowDisplay = checked
                    SettingsHelper.saveLanguageFollowDisplay(GlobalModel.languageFollowDisplay)
                    NativeTray.setLanguageFollowDisplay(GlobalModel.languageFollowDisplay)
                }

                Component.onCompleted: {
                    _languageFollowDisplay.checked = GlobalModel.languageFollowDisplay
                }
            }

            Connections {
                target: GlobalModel

                function onLanguageFollowDisplayChanged() {
                    _languageFollowDisplay.checked = GlobalModel.languageFollowDisplay
                }
            }

            // 浮窗透明度调节
            RowLayout {
                Layout.fillWidth: true
                spacing: 12
                enabled: _languageFollowDisplay.checked

                FluText {
                    text: qsTr("Overlay opacity")
                    Layout.preferredWidth: 120
                }

                Slider {
                    id: _overlayOpacity
                    Layout.fillWidth: true
                    from: 10
                    to: 50
                    stepSize: 1

                    Component.onCompleted: {
                        value = GlobalModel.languageOverlayOpacity
                    }

                    onPressedChanged: {
                        if (!pressed) {
                            GlobalModel.languageOverlayOpacity = Math.round(value)
                            SettingsHelper.saveLanguageOverlayOpacity(GlobalModel.languageOverlayOpacity)
                        }
                    }

                    onValueChanged: {
                        GlobalModel.languageOverlayOpacity = Math.round(value)
                    }

                    // 轨道: 未填充部分为灰色, 已填充部分为主题色, 直观显示当前比例
                    background: Rectangle {
                        x: _overlayOpacity.leftPadding
                        y: _overlayOpacity.topPadding + _overlayOpacity.availableHeight / 2 - height / 2
                        width: _overlayOpacity.availableWidth
                        height: 4
                        radius: 2
                        color: FluTheme.dark ? Qt.rgba(1, 1, 1, 0.2) : Qt.rgba(0, 0, 0, 0.15)

                        Rectangle {
                            width: _overlayOpacity.visualPosition * parent.width
                            height: parent.height
                            radius: 2
                            color: FluTheme.primaryColor
                        }
                    }

                    handle: Rectangle {
                        x: _overlayOpacity.leftPadding + _overlayOpacity.visualPosition * (_overlayOpacity.availableWidth - width)
                        y: _overlayOpacity.topPadding + _overlayOpacity.availableHeight / 2 - height / 2
                        implicitWidth: 18
                        implicitHeight: 18
                        radius: 9
                        color: FluTheme.dark ? Qt.rgba(0.18, 0.18, 0.18, 1) : "#FFFFFF"
                        border.width: _overlayOpacity.pressed ? 2 : 1
                        border.color: FluTheme.primaryColor

                        Rectangle {
                            anchors.centerIn: parent
                            width: _overlayOpacity.pressed ? 8 : (_overlayOpacity.hovered ? 11 : 9)
                            height: width
                            radius: width / 2
                            color: FluTheme.primaryColor
                            Behavior on width { NumberAnimation { duration: 100 } }
                        }
                    }
                }

                FluText {
                    text: Math.round(_overlayOpacity.value) + "%"
                    Layout.preferredWidth: 44
                }
            }

            // 全局快捷键: 开关浮窗显示 (应用不在前台也生效)
            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                FluCheckBox {
                    id: _hotkeyEnabled
                    text: qsTr("Toggle overlay with a global shortcut")

                    onClicked: {
                        GlobalModel.languageFollowHotkeyEnabled = checked
                        SettingsHelper.saveLanguageFollowHotkeyEnabled(checked)
                    }

                    Component.onCompleted: {
                        _hotkeyEnabled.checked = GlobalModel.languageFollowHotkeyEnabled
                    }
                }

                Item { Layout.fillWidth: true }

                FluShortcutPicker {
                    id: _hotkeyPicker
                    enabled: _hotkeyEnabled.checked
                    current: GlobalModel.languageFollowHotkey.split("+")

                    onAccepted: {
                        var sequence = current.join("+")
                        GlobalModel.languageFollowHotkey = sequence
                        SettingsHelper.saveLanguageFollowHotkey(sequence)
                    }
                }
            }
        }
    }

    FluFrame {
        Layout.fillWidth: true
        Layout.topMargin: 20
        height: 50
        padding: 10
        FluCheckBox {
            id: _isAlwaysCapLock
            text: qsTr("Whether turn on Cap Lock when Always ENG is on")
            anchors.verticalCenter: parent.verticalCenter
            onClicked: {
                GlobalModel.isAlwaysCapLock = checked
                SettingsHelper.saveCapLock(GlobalModel.isAlwaysCapLock)
            }
        }

        Component.onCompleted: {
            _isAlwaysCapLock.checked = GlobalModel.isAlwaysCapLock
        }
    }
}
