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
    readonly property bool wifiEnabled: wifiDevice.device && wifiDevice.state !== "unavailable"
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

    function launchShortcut(id) {
        if (id === "network") {
            shell.setAppearance({ overview: false })
            shell.command("open-settings", "network")
            return
        }
        if (id === "settings") {
            shell.setAppearance({ overview: false })
            shell.settingsOpen = true
            return
        }
        shell.launch(id)
    }

    Timer {
        interval: 1000
        running: true
        repeat: true
        triggeredOnStart: true
        onTriggered: {
            const now = new Date()
            dashboard.time = Qt.formatDateTime(now, Theme.clock24Hour ? "HH:mm" : "h:mm AP")
            dashboard.date = Qt.formatDateTime(now, "dddd, d MMMM yyyy")
        }
    }

    Rectangle {
        anchors.fill: parent
        color: moduleBackground
        radius: moduleRadius
        border.width: 1
        border.color: Qt.rgba(moduleAccent.r, moduleAccent.g, moduleAccent.b, 0.22)
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 22
        spacing: 14

        RowLayout {
            Layout.fillWidth: true
            Repeater {
                model: ["Dashboard", "Performance", "Workspaces"]
                ShellButton {
                    required property string modelData
                    required property int index
                    text: shell.tr(modelData)
                    active: dashboard.tab === index
                    Layout.fillWidth: true
                    onClicked: dashboard.tab = index
                }
            }
            ShellButton { text: "×"; Accessible.name: shell.tr("Close dashboard"); onClicked: shell.setAppearance({ overview: false }) }
        }

        Rectangle { Layout.fillWidth: true; height: 1; color: Theme.border }

        RowLayout {
            visible: dashboard.tab === 0
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 14

            ColumnLayout {
                Layout.preferredWidth: 330
                Layout.fillHeight: true
                spacing: 10

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 112
                    radius: 18
                    color: Theme.surface
                    border.width: 1
                    border.color: Theme.border
                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 14
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 1
                            Text { text: dashboard.time; font.pixelSize: 46; font.weight: Font.Light; font.family: Theme.font; color: moduleAccent }
                            Text { text: dashboard.date; color: Theme.muted; font.family: Theme.font; font.pixelSize: 12; elide: Text.ElideRight; Layout.fillWidth: true }
                        }
                        ShellButton { text: shell.tr("Calendar"); onClicked: shell.calendarOpen = !shell.calendarOpen }
                    }
                }

                Rectangle {
                    visible: dashboard.overviewConfig.quickControls ?? true
                    Layout.fillWidth: true
                    Layout.preferredHeight: 154
                    radius: 18
                    color: Theme.surface
                    border.width: 1
                    border.color: Theme.border
                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 12
                        spacing: 7
                        RowLayout {
                            Layout.fillWidth: true
                            Text { text: shell.tr("Volume"); color: Theme.text; font.family: Theme.font; Layout.fillWidth: true }
                            Text { text: String((dashboard.audio.output || {}).volume ?? 0) + "%"; color: Theme.accent; font.family: Theme.font }
                            ShellButton {
                                text: (dashboard.audio.output || {}).muted ? shell.tr("Unmute") : shell.tr("Mute")
                                onClicked: shell.command("audio", JSON.stringify({device:"output", mute:!((dashboard.audio.output || {}).muted ?? false)}))
                            }
                        }
                        SoftSlider {
                            Layout.fillWidth: true
                            from: 0
                            to: 100
                            stepSize: 1
                            value: (dashboard.audio.output || {}).volume ?? 0
                            enabled: (dashboard.audio.output || {}).available ?? false
                            onMoved: shell.command("audio", JSON.stringify({device:"output", volume:Math.round(value)}))
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            Text { text: shell.tr("Wi-Fi"); color: Theme.text; font.family: Theme.font; Layout.fillWidth: true }
                            ShellButton {
                                text: dashboard.wifiEnabled ? shell.tr("On") : shell.tr("Off")
                                active: dashboard.wifiEnabled
                                onClicked: shell.command("network", JSON.stringify({action:"wifi-enable", enabled:!dashboard.wifiEnabled}))
                            }
                            ShellButton {
                                text: shell.tr("Networks")
                                onClicked: {
                                    shell.setAppearance({ overview: false })
                                    shell.command("open-settings", "network")
                                }
                            }
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: 18
                    color: Theme.surface
                    border.width: 1
                    border.color: Theme.border
                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 12
                        spacing: 6
                        Text { text: shell.tr("Quick launch"); color: Theme.muted; font.family: Theme.font; font.pixelSize: 11 }
                        Flow {
                            Layout.fillWidth: true
                            spacing: 6
                            Repeater {
                                model: dashboard.shortcuts
                                ShellButton {
                                    required property string modelData
                                    text: dashboard.shortcutLabel(modelData)
                                    onClicked: dashboard.launchShortcut(modelData)
                                }
                            }
                        }
                        Text {
                            Layout.fillWidth: true
                            text: shell.tr("Customize these launchers in Settings → Shell modules → overview.")
                            color: Theme.muted
                            font.family: Theme.font
                            font.pixelSize: 10
                            wrapMode: Text.WordWrap
                        }
                        Item { Layout.fillHeight: true }
                        Flow {
                            Layout.fillWidth: true
                            spacing: 5
                            Repeater {
                                model: dashboard.savedWifi
                                ShellButton {
                                    required property var modelData
                                    text: modelData.name
                                    active: modelData.state === "activated"
                                    onClicked: shell.command("network", JSON.stringify({action:"connection-up", name:modelData.name}))
                                }
                            }
                        }
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 10

                MediaCard {
                    visible: dashboard.overviewConfig.showMedia ?? true
                    shell: dashboard.shell
                    Layout.fillWidth: true
                    Layout.preferredHeight: visible ? 180 : 0
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: 18
                    color: Theme.surface
                    border.width: 1
                    border.color: Theme.border
                    clip: true
                    Image {
                        anchors.fill: parent
                        source: shell.state.wallpaperImage || ""
                        fillMode: Image.PreserveAspectCrop
                        asynchronous: true
                        opacity: 0.28
                    }
                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 18
                        spacing: 6
                        Text {
                            text: (shell.state.appearance || {}).showHostDetails
                                ? (dashboard.stats.user || "user") + " @ " + (dashboard.stats.host || "linux")
                                : shell.tr("Your workspace")
                            color: moduleForeground
                            font.family: Theme.font
                            font.pixelSize: 19
                        }
                        Text { text: (dashboard.stats.os || "Linux") + " · " + (shell.state.graphicsApi || "OpenGL"); color: Theme.muted; font.family: Theme.font; font.pixelSize: 11 }
                        Text { text: shell.tr((shell.state.network || {}).label || "Checking network"); color: Theme.muted; font.family: Theme.font; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                        Item { Layout.fillHeight: true }
                        RowLayout {
                            Layout.fillWidth: true
                            Text { text: shell.tr("Connected media apps use MPRIS, so Spotify, browser-based YouTube, VLC and compatible players share the same controls."); color: Theme.muted; font.family: Theme.font; font.pixelSize: 10; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                        }
                    }
                }
            }
        }

        ColumnLayout {
            visible: dashboard.tab === 1
            Layout.fillHeight: true
            Layout.fillWidth: true
            spacing: 15
            Text { text: dashboard.stats.cpuModel || "CPU"; color: moduleForeground; font.family: Theme.font; font.pixelSize: 17; elide: Text.ElideRight; Layout.fillWidth: true }
            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 12
                Repeater {
                    model: [["CPU", (dashboard.stats.cpuPercent || 0) + "%"], ["RAM", Number(dashboard.stats.memoryUsed || 0).toFixed(1) + " GiB"], ["DISK", Number(dashboard.stats.diskUsed || 0).toFixed(0) + " GiB"]]
                    Rectangle {
                        required property var modelData
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        radius: 22
                        color: Theme.surface
                        Column {
                            anchors.centerIn: parent
                            spacing: 12
                            Text { text: modelData[0]; color: Theme.muted; font.family: Theme.font; font.pixelSize: 12 }
                            Text { text: modelData[1]; color: moduleAccent; font.family: Theme.font; font.pixelSize: 29; font.weight: Font.Light }
                        }
                    }
                }
            }
            Text { text: "Kernel  " + (dashboard.stats.kernel || "—") + "    ·    " + shell.tr("Live system statistics"); color: Theme.muted; font.family: Theme.font; font.pixelSize: 12 }
            ShellButton { text: shell.tr("System monitor"); onClicked: shell.launch("monitor") }
        }

        RowLayout {
            visible: dashboard.tab === 2
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 14
            Repeater {
                model: (shell.state.appearance || {}).workspaceCount || 4
                Rectangle {
                    required property int index
                    property int count: shell.state.clients.filter(client => client.workspace === index && client.mapped).length
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: 24
                    color: shell.state.workspace === index ? Qt.rgba(moduleAccent.r, moduleAccent.g, moduleAccent.b, 0.22) : Theme.surface
                    Column {
                        anchors.centerIn: parent
                        spacing: 15
                        Rectangle { anchors.horizontalCenter: parent.horizontalCenter; width: shell.state.workspace === index ? 54 : 30; height: 10; radius: 5; color: shell.state.workspace === index ? moduleAccent : Theme.muted }
                        Text { text: count + " " + shell.tr("windows"); color: Theme.muted; font.family: Theme.font; font.pixelSize: 12 }
                    }
                    MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: shell.command("workspace", index) }
                }
            }
        }
    }
}
