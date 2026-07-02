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
        // 高度随内容自适应, 避免固定高度裁切
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
                    // 固定适中宽度, 拉满整行既不美观也不便于精确调节
                    Layout.preferredWidth: 280
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

                Item { Layout.fillWidth: true }
            }

            // 浮窗背景色: "auto" 跟随明暗主题, 其余为固定颜色
            RowLayout {
                Layout.fillWidth: true
                spacing: 12
                enabled: _languageFollowDisplay.checked

                FluText {
                    text: qsTr("Overlay color")
                    Layout.preferredWidth: 120
                }

                Row {
                    spacing: 8

                    Repeater {
                        model: ["auto", "#FFFFFF", "#1F1F1F", "#0F6CBD", "#0F7B0F", "#C50F1F", "#CA5010", "#886CE4"]

                        delegate: Rectangle {
                            width: 22
                            height: 22
                            radius: 11
                            color: modelData === "auto"
                                   ? (FluTheme.dark ? "#2B2B2B" : "#F3F3F3")
                                   : modelData
                            border.width: GlobalModel.languageOverlayColor === modelData ? 2 : 1
                            border.color: GlobalModel.languageOverlayColor === modelData
                                          ? FluTheme.primaryColor
                                          : (FluTheme.dark ? "#666666" : "#CCCCCC")

                            // "auto" 色块用 A 标识: 根据浮窗背后画面明暗自动调整底色
                            FluText {
                                visible: modelData === "auto"
                                anchors.centerIn: parent
                                text: "A"
                                font.pixelSize: 11
                            }

                            FluTooltip {
                                visible: modelData === "auto" && _swatchArea.containsMouse
                                text: qsTr("Auto: adapts to the brightness of the screen behind the overlay")
                                delay: 400
                            }

                            MouseArea {
                                id: _swatchArea
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    GlobalModel.languageOverlayColor = modelData
                                    SettingsHelper.saveLanguageOverlayColor(modelData)
                                }
                            }
                        }
                    }
                }

                Item { Layout.fillWidth: true }
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
}
