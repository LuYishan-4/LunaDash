import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Quickshell
import Quickshell.Wayland
import "../modules"
import "../components"
import "../style"

ModuleSurface {
    id: center
    moduleId: "overview"
    anchors.top: true
    anchors.right: true
    margins.top: Theme.panelTopInset + moduleMargin
    margins.right: Theme.panelRightInset + moduleMargin
    implicitWidth: moduleWidth(660)
    implicitHeight: moduleHeight(580)
    exclusionMode: ExclusionMode.Ignore
    WlrLayershell.layer: WlrLayer.Top
    WlrLayershell.namespace: "lunadash-control-center"
    color: "transparent"
    property int tab: config.showMedia === false && shell.controlCenterTab === 1 ? 0 : shell.controlCenterTab
    readonly property var config: specification.config || ({})
    readonly property var stats: shell.state.system || ({})
    readonly property var output: (shell.state.audio || {}).output || ({})
    readonly property var input: (shell.state.audio || {}).input || ({})
    readonly property var tabs: [
        {
            name: "Control center",
            icon: "apps"
        },
        {
            name: "Media",
            icon: "music"
        },
        {
            name: "Sound",
            icon: "sound"
        },
        {
            name: "System",
            icon: "monitor"
        }
    ]

    Rectangle {
        anchors.fill: parent
        radius: Theme.radiusLarge
        color: Theme.surfaceStrong
        border.width: 1
        border.color: Theme.hairline
        clip: true
        RowLayout {
            anchors.fill: parent
            anchors.margins: 14
            spacing: 12
            Rectangle {
                Layout.preferredWidth: 50
                Layout.fillHeight: true
                radius: Theme.radiusMedium
                color: Theme.surface
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 5
                    spacing: 8
                    Repeater {
                        model: center.tabs
                        ShellButton {
                            required property var modelData
                            required property int index
                            Layout.fillWidth: true
                            Layout.preferredHeight: 42
                            iconName: modelData.icon
                            active: center.tab === index
                            visible: index !== 1 || (center.config.showMedia ?? true)
                            toolTip: shell.tr(modelData.name)
                            Accessible.name: toolTip
                            onClicked: center.tab = index
                        }
                    }
                    Item {
                        Layout.fillHeight: true
                    }
                    ShellButton {
                        Layout.fillWidth: true
                        iconName: "settings"
                        Accessible.name: shell.tr("Settings")
                        onClicked: {
                            shell.setAppearance({
                                overview: false
                            });
                            shell.openSettingsPage("general");
                        }
                    }
                    ShellButton {
                        Layout.fillWidth: true
                        iconName: "power"
                        Accessible.name: shell.tr("Session controls")
                        onClicked: {
                            shell.setAppearance({
                                overview: false
                            });
                            shell.logoutOpen = true;
                        }
                    }
                }
            }
            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumWidth: 0
                spacing: 12
                RowLayout {
                    Layout.fillWidth: true
                    Text {
                        Layout.fillWidth: true
                        text: shell.tr(center.tabs[center.tab].name)
                        color: Theme.text
                        font.family: Theme.font
                        font.pixelSize: 18
                        font.weight: Font.DemiBold
                        elide: Text.ElideRight
                    }
                    ShellButton {
                        iconName: "close"
                        Accessible.name: shell.tr("Close")
                        onClicked: shell.setAppearance({
                            overview: false
                        })
                    }
                }
                MediaCard {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    shell: center.shell
                    visible: center.tab === 1 && (center.config.showMedia ?? true)
                }
                ScrollView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    visible: center.tab !== 1
                    clip: true
                    contentWidth: availableWidth
                    ColumnLayout {
                        width: parent.width
                        spacing: 12
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 124
                            visible: center.tab === 0
                            radius: Theme.radiusMedium
                            color: Theme.surfaceElevated
                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: 16
                                Text {
                                    Layout.fillWidth: true
                                    text: time.text
                                    color: Theme.accent
                                    font.family: Theme.font
                                    font.pixelSize: 42
                                    font.weight: Font.DemiBold
                                    elide: Text.ElideRight
                                }
                                Text {
                                    Layout.fillWidth: true
                                    text: time.date
                                    color: Theme.muted
                                    font.family: Theme.font
                                    elide: Text.ElideRight
                                }
                            }
                            QtObject {
                                id: time
                                property string text: ""
                                property string date: ""
                            }
                            Timer {
                                interval: 1000
                                running: center.opened
                                repeat: true
                                triggeredOnStart: true
                                onTriggered: {
                                    time.text = Qt.formatDateTime(new Date(), Theme.clock24Hour ? "HH:mm" : "h:mm AP");
                                    time.date = Qt.formatDateTime(new Date(), "dddd, MMMM d");
                                }
                            }
                        }
                        GridLayout {
                            Layout.fillWidth: true
                            columns: center.width < 500 ? 1 : 2
                            columnSpacing: 10
                            rowSpacing: 10
                            visible: center.tab === 0 && (center.config.quickControls ?? true)
                            ShellButton {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 68
                                iconName: "network"
                                text: shell.tr("Network")
                                active: (shell.state.network || {}).connected ?? false
                                onClicked: {
                                    shell.setAppearance({
                                        overview: false
                                    });
                                    shell.wifiPopupOpen = true;
                                }
                            }
                            ShellButton {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 68
                                iconName: "bluetooth"
                                text: shell.tr("Bluetooth")
                                onClicked: {
                                    shell.setAppearance({
                                        overview: false
                                    });
                                    shell.openSettingsPage("bluetooth");
                                }
                            }
                            ShellButton {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 68
                                iconName: "moon"
                                text: shell.tr("Eye care")
                                active: (shell.state.appearance || {}).eyeCare ?? false
                                onClicked: shell.command("eye-care", "toggle")
                            }
                            ShellButton {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 68
                                iconName: "appearance"
                                text: Theme.dark ? shell.tr("Dark") : shell.tr("Light")
                                onClicked: shell.setAppearance({
                                    themeMode: Theme.dark ? "light" : "dark"
                                })
                            }
                            ShellButton {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 68
                                iconName: "terminal"
                                text: shell.tr("Scratchpad")
                                onClicked: {
                                    shell.setAppearance({
                                        overview: false
                                    });
                                    shell.command("scratchpad", "");
                                }
                            }
                            ShellButton {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 68
                                iconName: "files"
                                text: shell.tr("Wallpapers")
                                onClicked: {
                                    shell.setAppearance({
                                        overview: false
                                    });
                                    shell.command("choose-wallpaper", "");
                                }
                            }
                        }
                        Repeater {
                            model: center.tab === 0 || center.tab === 2 ? ["output", "input"] : []
                            Rectangle {
                                required property string modelData
                                readonly property var device: modelData === "output" ? center.output : center.input
                                Layout.fillWidth: true
                                Layout.preferredHeight: 96
                                radius: Theme.radiusMedium
                                color: Theme.surface
                                ColumnLayout {
                                    anchors.fill: parent
                                    anchors.margins: 12
                                    RowLayout {
                                        Layout.fillWidth: true
                                        Text {
                                            Layout.fillWidth: true
                                            text: shell.tr(modelData === "output" ? "Output volume" : "Microphone")
                                            color: Theme.text
                                            font.family: Theme.font
                                            elide: Text.ElideRight
                                        }
                                        ShellButton {
                                            text: device.muted ? shell.tr("Unmute") : shell.tr("Mute")
                                            active: device.muted ?? false
                                            enabled: device.available !== false
                                            onClicked: shell.command("audio", JSON.stringify({
                                                device: modelData,
                                                mute: !device.muted
                                            }))
                                        }
                                    }
                                    SoftSlider {
                                        Layout.fillWidth: true
                                        from: 0
                                        to: 100
                                        value: device.volume || 0
                                        enabled: device.available !== false
                                        onPressedChanged: if (!pressed)
                                            shell.command("audio", JSON.stringify({
                                                device: modelData,
                                                volume: Math.round(value)
                                            }))
                                    }
                                }
                            }
                        }
                        Repeater {
                            model: center.tab === 3 ? [
                                {
                                    name: "CPU",
                                    value: Math.round(center.stats.cpuPercent || 0) + "%",
                                    detail: center.stats.cpuModel || ""
                                },
                                {
                                    name: "Memory",
                                    value: Math.round(center.stats.memoryPercent || 0) + "%",
                                    detail: Number(center.stats.memoryUsed || 0).toFixed(1) + " / " + Number(center.stats.memoryTotal || 0).toFixed(1) + " GiB"
                                },
                                {
                                    name: "Storage",
                                    value: Math.round(center.stats.diskPercent || 0) + "%",
                                    detail: center.stats.diskDevice || ""
                                }
                            ] : []
                            Rectangle {
                                required property var modelData
                                Layout.fillWidth: true
                                Layout.preferredHeight: 110
                                radius: Theme.radiusMedium
                                color: Theme.surface
                                ColumnLayout {
                                    anchors.fill: parent
                                    anchors.margins: 16
                                    Text {
                                        Layout.fillWidth: true
                                        text: shell.tr(modelData.name) + "  " + modelData.value
                                        color: Theme.accent
                                        font.family: Theme.font
                                        font.pixelSize: 24
                                        elide: Text.ElideRight
                                    }
                                    Text {
                                        Layout.fillWidth: true
                                        text: modelData.detail
                                        color: Theme.muted
                                        font.family: Theme.font
                                        elide: Text.ElideRight
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
