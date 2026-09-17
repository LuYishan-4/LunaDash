import "../modules"
import QtQuick
import QtQuick.Layouts
import Quickshell
import Quickshell.Wayland
import "../components"
import "../style"

ModuleSurface {
    id: dashboard
    moduleId: "overview"
    property var stats: shell.state.system || ({})
    property int tab: 0
    property string time: ""
    property string date: ""
    readonly property var overviewConfig: specification.config || ({})
    readonly property var shortcuts: overviewConfig.shortcuts || ["files", "terminal", "settings"]
    readonly property var audio: shell.state.audio || ({})
    readonly property var network: shell.state.network || ({})
    readonly property var wifiDevice: (network.devices || []).find(device => String(device.type).toLowerCase() === "wifi") || ({})
    readonly property bool wifiEnabled: Boolean(wifiDevice.device) && wifiDevice.state !== "unavailable"
    readonly property var savedWifi: (network.connections || []).filter(connection => {
        const type = String(connection.type || "").toLowerCase()
        return type.includes("wireless") || type.includes("wifi")
    }).slice(0, 3)

    anchors.top: true
    margins.top: Theme.barHeight + moduleMargin
    implicitWidth: moduleWidth(920)
    implicitHeight: moduleHeight(590)
    exclusionMode: ExclusionMode.Ignore
    WlrLayershell.layer: WlrLayer.Top
    WlrLayershell.namespace: "lunadash-overview"
    color: "transparent"

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
        color: moduleBackground
        border.width: 1
        border.color: Qt.rgba(moduleAccent.r, moduleAccent.g, moduleAccent.b, 0.35)
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 14

        RowLayout {
            Layout.fillWidth: true
            spacing: 12
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2
                Text { text: dashboard.time; color: moduleForeground; font.family: Theme.font; font.pixelSize: 30; font.weight: Font.Bold }
                Text { text: dashboard.date; color: Theme.muted; font.family: Theme.font; font.pixelSize: 13 }
            }
            Repeater {
                model: dashboard.shortcuts
                delegate: Rectangle {
                    required property var modelData
                    width: 42; height: 42; radius: 21
                    color: shortcutMouse.containsMouse ? Qt.rgba(moduleAccent.r, moduleAccent.g, moduleAccent.b, 0.22) : Theme.control
                    LineIcon { anchors.centerIn: parent; width: 20; height: 20; name: dashboard.shortcutIcon(String(modelData)); ink: moduleAccent }
                    MouseArea { id: shortcutMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: shell.launch(String(modelData)) }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 14
            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                radius: Theme.radius
                color: Theme.control
                ColumnLayout {
                    anchors.fill: parent; anchors.margins: 14; spacing: 10
                    Text { text: shell.tr("System"); color: moduleForeground; font.family: Theme.font; font.pixelSize: 16; font.weight: Font.DemiBold }
                    Text { text: "CPU  " + Math.round(dashboard.stats.cpuPercent || 0) + "%"; color: moduleForeground; font.family: Theme.font }
                    Text { text: shell.tr("Memory") + "  " + Math.round(dashboard.stats.memoryPercent || 0) + "%"; color: moduleForeground; font.family: Theme.font }
                    Text { text: shell.tr("Battery") + "  " + ((dashboard.stats.batteryPercent ?? -1) >= 0 ? dashboard.stats.batteryPercent + "%" : "—"); color: moduleForeground; font.family: Theme.font }
                }
            }
            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                radius: Theme.radius
                color: Theme.control
                ColumnLayout {
                    anchors.fill: parent; anchors.margins: 14; spacing: 10
                    Text { text: shell.tr("Network"); color: moduleForeground; font.family: Theme.font; font.pixelSize: 16; font.weight: Font.DemiBold }
                    Text { text: (dashboard.network.connected ? shell.tr("Connected") : shell.tr("Disconnected")); color: dashboard.network.internet ? Theme.accent : Theme.muted; font.family: Theme.font }
                    Text { text: dashboard.wifiDevice.name || dashboard.wifiDevice.device || shell.tr("Wi-Fi"); color: moduleForeground; font.family: Theme.font }
                    Text { text: dashboard.wifiEnabled ? shell.tr("Enabled") : shell.tr("Disabled"); color: dashboard.wifiEnabled ? Theme.accent : Theme.muted; font.family: Theme.font }
                }
            }
        }

        MediaCard {
            Layout.fillWidth: true
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
