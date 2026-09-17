import "../modules"
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Quickshell
import Quickshell.Wayland
import "../components"
import "../style"

ModuleSurface {
    id: dashboard
    moduleId: "overview"
    property var stats: shell.state.system || ({})
    property string time: ""
    property string date: ""
    property real reveal: opened ? 1 : 0
    readonly property var overviewConfig: specification.config || ({})
    readonly property var shortcuts: overviewConfig.shortcuts || ["files", "terminal", "settings"]
    readonly property bool showMedia: overviewConfig.showMedia ?? true
    readonly property bool quickControls: overviewConfig.quickControls ?? true
    readonly property var audio: shell.state.audio || ({})
    readonly property var network: shell.state.network || ({})
    readonly property var wifiDevice: (network.devices || []).find(device => String(device.type).toLowerCase() === "wifi") || ({})
    readonly property bool wifiEnabled: Boolean(wifiDevice.device) && wifiDevice.state !== "unavailable"

    anchors.top: true
    margins.top: Theme.barHeight + moduleMargin + 8
    implicitWidth: moduleWidth(980)
    implicitHeight: moduleHeight(650)
    exclusionMode: ExclusionMode.Ignore
    WlrLayershell.layer: WlrLayer.Top
    WlrLayershell.namespace: "lunadash-overview"
    color: "transparent"

    Behavior on reveal { NumberAnimation { duration: Math.max(180, Theme.motion * 1.4); easing.type: Easing.OutCubic } }

    function shortcutLabel(id) {
        if (id === "files") return shell.tr("Files")
        if (id === "terminal") return shell.tr("Terminal")
        if (id === "settings") return shell.tr("Desktop settings")
        if (id === "monitor") return shell.tr("System monitor")
        if (id === "network") return shell.tr("Network settings")
        if (id === "plugins") return shell.tr("Plugins")
        return id
    }
    function shortcutIcon(id) {
        if (id === "files") return "files"
        if (id === "terminal") return "terminal"
        if (id === "settings") return "settings"
        if (id === "monitor") return "monitor"
        if (id === "network") return "network"
        if (id === "plugins") return "apps"
        return "apps"
    }

    Rectangle {
        anchors.fill: parent
        radius: Theme.radiusLarge
        color: Theme.surfaceOpaque
        border.width: 1
        border.color: Qt.rgba(moduleAccent.r, moduleAccent.g, moduleAccent.b, 0.42)
        opacity: dashboard.reveal
        scale: 0.96 + 0.04 * dashboard.reveal
        transformOrigin: Item.Top
        Behavior on scale { NumberAnimation { duration: Math.max(180, Theme.motion); easing.type: Easing.OutCubic } }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 14
        opacity: dashboard.reveal

        RowLayout {
            Layout.fillWidth: true
            spacing: 14

            Rectangle {
                width: 62
                height: 62
                radius: 31
                color: Qt.rgba(Theme.secondaryAccent.r, Theme.secondaryAccent.g, Theme.secondaryAccent.b, 0.62)
                border.width: 2
                border.color: Theme.accent
                clip: true
                Image {
                    anchors.fill: parent
                    source: dashboard.stats.avatar || ""
                    fillMode: Image.PreserveAspectCrop
                    asynchronous: true
                    visible: source.toString().length > 0 && status !== Image.Error
                }
                LunaDashLogo {
                    anchors.centerIn: parent
                    width: 38
                    height: 38
                    visible: !dashboard.stats.avatar
                    animated: false
                    primaryColor: Theme.accent
                    secondaryColor: Theme.secondaryAccent
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 1
                Text {
                    Layout.fillWidth: true
                    text: dashboard.stats.displayName || dashboard.stats.user || shell.tr("Welcome")
                    color: moduleForeground
                    font.family: Theme.font
                    font.pixelSize: 17
                    font.weight: Font.DemiBold
                    elide: Text.ElideRight
                }
                Text { text: dashboard.time; color: moduleForeground; font.family: Theme.font; font.pixelSize: 30; font.weight: Font.Bold }
                Text { text: dashboard.date; color: Theme.muted; font.family: Theme.font; font.pixelSize: 12 }
            }

            Repeater {
                model: dashboard.shortcuts
                delegate: Rectangle {
                    id: shortcutTile
                    required property var modelData
                    width: 46
                    height: 46
                    radius: 15
                    color: shortcutMouse.containsMouse
                        ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.22)
                        : Qt.rgba(Theme.secondaryAccent.r, Theme.secondaryAccent.g, Theme.secondaryAccent.b, 0.42)
                    border.width: 1
                    border.color: shortcutMouse.containsMouse ? Theme.accent : Theme.border
                    scale: shortcutMouse.pressed ? 0.92 : shortcutMouse.containsMouse ? 1.05 : 1
                    Behavior on color { ColorAnimation { duration: Theme.motion } }
                    Behavior on scale { NumberAnimation { duration: Math.max(90, Theme.motion); easing.type: Easing.OutCubic } }
                    LineIcon { anchors.centerIn: parent; width: 21; height: 21; name: dashboard.shortcutIcon(String(modelData)); ink: Theme.accent }
                    MouseArea { id: shortcutMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: shell.launch(String(modelData)) }
                    ToolTip.visible: shortcutMouse.containsMouse
                    ToolTip.delay: 400
                    ToolTip.text: dashboard.shortcutLabel(String(modelData))
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 14

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 132
                radius: 20
                color: Qt.rgba(Theme.secondaryAccent.r, Theme.secondaryAccent.g, Theme.secondaryAccent.b, 0.34)
                border.width: 1
                border.color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.24)
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 14
                    spacing: 8
                    RowLayout {
                        Layout.fillWidth: true
                        LineIcon { width: 18; height: 18; name: "monitor"; ink: Theme.accent }
                        Text { Layout.fillWidth: true; text: shell.tr("System"); color: Theme.text; font.family: Theme.font; font.pixelSize: 15; font.weight: Font.DemiBold }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Text { Layout.fillWidth: true; text: "CPU"; color: Theme.muted; font.family: Theme.font }
                        Text { text: Math.round(dashboard.stats.cpuPercent || 0) + "%"; color: Theme.text; font.family: Theme.font; font.weight: Font.DemiBold }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Text { Layout.fillWidth: true; text: shell.tr("Memory"); color: Theme.muted; font.family: Theme.font }
                        Text { text: Math.round(dashboard.stats.memoryPercent || 0) + "%"; color: Theme.text; font.family: Theme.font; font.weight: Font.DemiBold }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Text { Layout.fillWidth: true; text: shell.tr("Battery"); color: Theme.muted; font.family: Theme.font }
                        Text { text: (dashboard.stats.batteryPercent ?? -1) >= 0 ? dashboard.stats.batteryPercent + "%" : "—"; color: Theme.text; font.family: Theme.font; font.weight: Font.DemiBold }
                    }
                }
            }

            Rectangle {
                visible: dashboard.quickControls
                Layout.fillWidth: true
                Layout.preferredHeight: 132
                radius: 20
                color: Qt.rgba(Theme.secondaryAccent.r, Theme.secondaryAccent.g, Theme.secondaryAccent.b, 0.34)
                border.width: 1
                border.color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.24)
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 14
                    spacing: 8
                    RowLayout {
                        Layout.fillWidth: true
                        LineIcon { width: 18; height: 18; name: "sound"; ink: Theme.accent }
                        Text { Layout.fillWidth: true; text: shell.tr("Volume"); color: Theme.text; font.family: Theme.font; font.pixelSize: 15; font.weight: Font.DemiBold }
                        Text { text: Math.round((dashboard.audio.output || {}).volume || 0) + "%"; color: Theme.muted; font.family: Theme.font }
                    }
                    SoftSlider {
                        Layout.fillWidth: true
                        from: 0
                        to: 100
                        stepSize: 1
                        value: (dashboard.audio.output || {}).volume || 0
                        enabled: Boolean((dashboard.audio.output || {}).available) && !dashboard.audio.busy
                        onMoved: shell.command("audio", JSON.stringify({device:"output", volume:Math.round(value)}))
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        ShellButton {
                            text: (dashboard.audio.output || {}).muted ? shell.tr("Unmute") : shell.tr("Mute")
                            active: (dashboard.audio.output || {}).muted ?? false
                            onClicked: shell.command("audio", JSON.stringify({device:"output", mute:!((dashboard.audio.output || {}).muted ?? false)}))
                        }
                        Item { Layout.fillWidth: true }
                        ShellButton { text: shell.tr("More"); onClicked: shell.volumePopupOpen = true }
                    }
                }
            }

            Rectangle {
                visible: dashboard.quickControls
                Layout.fillWidth: true
                Layout.preferredHeight: 132
                radius: 20
                color: Qt.rgba(Theme.secondaryAccent.r, Theme.secondaryAccent.g, Theme.secondaryAccent.b, 0.34)
                border.width: 1
                border.color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.24)
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 14
                    spacing: 8
                    RowLayout {
                        Layout.fillWidth: true
                        LineIcon { width: 18; height: 18; name: "network"; ink: dashboard.network.internet ? Theme.accent : Theme.muted }
                        Text { Layout.fillWidth: true; text: shell.tr("Wi-Fi"); color: Theme.text; font.family: Theme.font; font.pixelSize: 15; font.weight: Font.DemiBold }
                        SoftSwitch {
                            checked: dashboard.wifiEnabled
                            onToggled: shell.command("network", JSON.stringify({action:"wifi-enable", enabled:checked}))
                        }
                    }
                    Text {
                        Layout.fillWidth: true
                        text: dashboard.network.connected ? shell.tr("Connected") : shell.tr("Disconnected")
                        color: dashboard.network.internet ? Theme.accent : Theme.muted
                        font.family: Theme.font
                    }
                    Text {
                        Layout.fillWidth: true
                        text: dashboard.wifiDevice.connection || dashboard.wifiDevice.name || dashboard.wifiDevice.device || shell.tr("No Wi-Fi connection")
                        color: Theme.muted
                        font.family: Theme.font
                        font.pixelSize: 11
                        elide: Text.ElideRight
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Item { Layout.fillWidth: true }
                        ShellButton { text: shell.tr("Networks"); onClicked: shell.wifiPopupOpen = true }
                    }
                }
            }
        }

        MediaCard {
            visible: dashboard.showMedia
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 190
            shell: dashboard.shell
        }
    }

    Timer {
        interval: 1000
        repeat: true
        running: true
        triggeredOnStart: true
        onTriggered: {
            const now = new Date()
            dashboard.time = Qt.formatDateTime(now, Theme.clock24Hour ? "HH:mm:ss" : "h:mm:ss AP")
            dashboard.date = Qt.formatDateTime(now, "yyyy/MM/dd dddd")
        }
    }
}
