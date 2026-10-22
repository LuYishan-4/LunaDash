import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components"
import "../style"

Rectangle {
    id: welcome
    required property var shell
    signal finished()
    signal settingsRequested(string page)
    readonly property bool compact: width < 660
    readonly property var preferences: shell.state.appearance || ({})
    color: Theme.surfaceStrong
    radius: Theme.radius
    border.width: 1
    border.color: Theme.hairline
    clip: true
    Component.onCompleted: Qt.callLater(function() { finishButton.forceActiveFocus() })

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: welcome.compact ? 16 : 24
        spacing: 16
        Text {
            Layout.fillWidth: true
            visible: text.length > 0
            text: welcome.shell.state.welcomeError || ""
            color: Theme.danger
            font.family: Theme.font
            wrapMode: Text.WordWrap
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: 16
            LunaDashLogo {
                Layout.preferredWidth: 44
                Layout.preferredHeight: 44
                animated: false
            }
            ColumnLayout {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                spacing: 4
                Text {
                    Layout.fillWidth: true
                    text: welcome.shell.tr("Welcome to LunaDash")
                    font.family: Theme.font
                    font.pixelSize: welcome.compact ? 22 : 28
                    font.weight: Font.DemiBold
                    color: Theme.text
                    elide: Text.ElideRight
                }
                Text {
                    Layout.fillWidth: true
                    text: "LUNADASH  " + (welcome.shell.state.version || "1.0.1a")
                    font.family: Theme.font
                    font.pixelSize: 11
                    color: Theme.accent
                    elide: Text.ElideRight
                }
            }
        }
        ScrollView {
            id: scroll
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: availableWidth
            clip: true
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
            ColumnLayout {
                width: scroll.availableWidth
                spacing: 14
                Text {
                    Layout.fillWidth: true
                    text: welcome.shell.tr("Make yourself at home. Choose a comfortable appearance and learn your way around.")
                    color: Theme.muted
                    font.family: Theme.font
                    font.pixelSize: 14
                    wrapMode: Text.WordWrap
                }
                WelcomeCard {
                    Text {
                        Layout.fillWidth: true
                        text: welcome.shell.tr("Make this space yours")
                        font.family: Theme.font
                        font.pixelSize: 17
                        font.bold: true
                        color: Theme.text
                        wrapMode: Text.WordWrap
                    }
                    GridLayout {
                        Layout.fillWidth: true
                        columns: welcome.compact ? 1 : 2
                        columnSpacing: 16
                        rowSpacing: 10
                        Text {
                            Layout.fillWidth: true
                            text: welcome.shell.tr("Interface language")
                            color: Theme.muted
                            font.family: Theme.font
                            wrapMode: Text.WordWrap
                        }
                        StyledComboBox {
                            objectName: "welcomeLanguage"
                            Layout.fillWidth: true
                            Layout.minimumWidth: 0
                            translationContext: welcome.shell
                            model: ["English", "Traditional Chinese", "Simplified Chinese", "Japanese"]
                            readonly property var locales: ["en_US", "zh_TW", "zh_CN", "ja_JP"]
                            currentIndex: Math.max(0, locales.indexOf(welcome.shell.state.language || "en_US"))
                            onActivated: welcome.shell.command("language", locales[currentIndex])
                        }
                        Text {
                            Layout.fillWidth: true
                            text: welcome.shell.tr("Visual effects")
                            color: Theme.muted
                            font.family: Theme.font
                            wrapMode: Text.WordWrap
                        }
                        ShellButton {
                            objectName: "welcomeReducedMotion"
                            Layout.fillWidth: true
                            active: welcome.preferences.animations === false
                            text: welcome.shell.tr("Reduced motion")
                            onClicked: welcome.shell.command("appearance", JSON.stringify({animations: welcome.preferences.animations === false}))
                        }
                    }
                }
                GridLayout {
                    Layout.fillWidth: true
                    columns: welcome.compact ? 1 : 2
                    columnSpacing: 14
                    rowSpacing: 14
                    WelcomeCard {
                        Layout.alignment: Qt.AlignTop
                        Text {
                            Layout.fillWidth: true
                            text: welcome.shell.tr("Your first steps")
                            font.family: Theme.font
                            font.pixelSize: 17
                            font.bold: true
                            color: Theme.text
                            wrapMode: Text.WordWrap
                        }
                        Repeater {
                            model: [
                                {page: "appearance", label: "Appearance"},
                                {page: "applications", label: "Default applications"},
                                {page: "plugins", label: "Plugins"}
                            ]
                            ShellButton {
                                required property var modelData
                                Layout.fillWidth: true
                                text: welcome.shell.tr(modelData.label)
                                onClicked: welcome.settingsRequested(modelData.page)
                            }
                        }
                        Text {
                            Layout.fillWidth: true
                            text: welcome.shell.tr("Settings stay available from the panel whenever you need them.")
                            font.family: Theme.font
                            font.pixelSize: 12
                            color: Theme.muted
                            wrapMode: Text.WordWrap
                        }
                    }
                    WelcomeCard {
                        Layout.alignment: Qt.AlignTop
                        Text {
                            Layout.fillWidth: true
                            text: welcome.shell.tr("Find your rhythm")
                            font.family: Theme.font
                            font.pixelSize: 17
                            font.bold: true
                            color: Theme.text
                            wrapMode: Text.WordWrap
                        }
                        Repeater {
                            model: [
                                {action: "launchLauncher", label: "Open launcher"},
                                {action: "launchTerminal", label: "Open terminal"},
                                {action: "launchFiles", label: "Open files"},
                                {action: "chooseWallpaper", label: "Wallpaper library"}
                            ]
                            RowLayout {
                                required property var modelData
                                Layout.fillWidth: true
                                Text {
                                    Layout.fillWidth: true
                                    Layout.minimumWidth: 0
                                    text: welcome.shell.tr(modelData.label)
                                    color: Theme.text
                                    font.family: Theme.font
                                    font.pixelSize: 12
                                    wrapMode: Text.WordWrap
                                }
                                Text {
                                    Layout.maximumWidth: Math.max(80, welcome.width / 5)
                                    text: (welcome.shell.state.shortcuts || {})[modelData.action] || welcome.shell.tr("Disabled")
                                    color: Theme.accent
                                    font.family: Theme.font
                                    font.pixelSize: 11
                                    wrapMode: Text.WrapAnywhere
                                }
                            }
                        }
                        ShellButton {
                            Layout.fillWidth: true
                            text: welcome.shell.tr("Keyboard shortcuts")
                            onClicked: welcome.settingsRequested("shortcuts")
                        }
                    }
                }
                Text {
                    Layout.fillWidth: true
                    text: welcome.shell.tr("Development preview. Some session and protocol features are still being improved.")
                    color: Theme.muted
                    font.family: Theme.font
                    font.pixelSize: 12
                    wrapMode: Text.WordWrap
                }
            }
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: 10
            ShellButton {
                Layout.fillWidth: true
                text: welcome.shell.tr("Documentation")
                quiet: true
                onClicked: welcome.shell.command("open-url", "https://luyishan-4.github.io/LunaDash/")
            }
            ShellButton {
                id: finishButton
                objectName: "welcomeFinish"
                focus: true
                Layout.fillWidth: true
                text: welcome.shell.tr("Start desktop")
                active: true
                onClicked: welcome.finished()
            }
        }
    }
}
