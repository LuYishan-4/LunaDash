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
    property int tab: 0
    property string time: ""
    property string date: ""
    property bool closeArmed: false

    readonly property var network: shell.state.network || ({})
    readonly property var weather: shell.state.weather || ({})
    readonly property var updateInfo: shell.state.update || ({})
    readonly property string versionText: updateInfo.version || updateInfo.currentVersion || "1.0.0"
    readonly property string weatherLocation: weather.location || weather.city || shell.tr("Weather")
    readonly property string weatherCondition: weather.condition || weather.summary || shell.tr("Weather service is not configured")
    readonly property string weatherTemperature: weather.temperature !== undefined ? String(weather.temperature) + "°" : "—"

    anchors.top: true
    anchors.left: true
    margins.top: Theme.barHeight + moduleMargin + 8
    margins.left: Math.max(moduleMargin, Math.round(((screen ? screen.width : 1440) - implicitWidth) / 2))
    implicitWidth: moduleWidth(1060)
    implicitHeight: moduleHeight(650)
    exclusionMode: ExclusionMode.Ignore
    WlrLayershell.layer: WlrLayer.Top
    WlrLayershell.namespace: "lunadash-overview"
    color: "transparent"

    HoverHandler {
        id: dashboardHover
        onHoveredChanged: {
            if (hovered) {
                dashboard.closeArmed = true
                closeTimer.stop()
            } else if (dashboard.closeArmed) {
                closeTimer.restart()
            }
        }
    }
    Timer {
        id: armCloseTimer
        interval: 520
        onTriggered: {
            dashboard.closeArmed = true
            if (!dashboardHover.hovered)
                closeTimer.restart()
        }
    }
    Timer {
        id: closeTimer
        interval: 260
        onTriggered: if (dashboard.opened && !dashboardHover.hovered)
            shell.setAppearance({overview: false})
    }
    onOpenedChanged: {
        if (opened) {
            closeArmed = false
            armCloseTimer.restart()
        } else {
            closeArmed = false
            armCloseTimer.stop()
            closeTimer.stop()
        }
    }

    Rectangle {
        anchors.fill: parent
        radius: Theme.radiusLarge
        color: Qt.rgba(Theme.surfaceOpaque.r, Theme.surfaceOpaque.g, Theme.surfaceOpaque.b, 0.97)
        border.width: 1
        border.color: Qt.rgba(Theme.starlight.r, Theme.starlight.g, Theme.starlight.b, 0.22)
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Repeater {
                model: [
                    ["Dashboard", "apps"],
                    ["Media", "sound"],
                    ["Performance", "monitor"],
                    ["Weather", "weather"]
                ]

                Rectangle {
                    required property var modelData
                    required property int index
                    Layout.fillWidth: true
                    Layout.preferredHeight: 48
                    radius: 14
                    color: dashboard.tab === index
                        ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.14)
                        : tabMouse.containsMouse
                            ? Qt.rgba(Theme.starlight.r, Theme.starlight.g, Theme.starlight.b, 0.07)
                            : "transparent"
                    border.width: dashboard.tab === index ? 1 : 0
                    border.color: Qt.rgba(Theme.moon.r, Theme.moon.g, Theme.moon.b, 0.42)

                    Row {
                        anchors.centerIn: parent
                        spacing: 8
                        LineIcon {
                            width: 18
                            height: 18
                            anchors.verticalCenter: parent.verticalCenter
                            name: modelData[1]
                            ink: dashboard.tab === index ? Theme.moon : Theme.muted
                        }
                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: shell.tr(modelData[0])
                            color: dashboard.tab === index ? Theme.text : Theme.muted
                            font.family: Theme.font
                            font.pixelSize: 12
                            font.weight: dashboard.tab === index ? Font.DemiBold : Font.Normal
                        }
                    }

                    MouseArea {
                        id: tabMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onEntered: dashboard.tab = index
                    onClicked: dashboard.tab = index
                    }
                    Behavior on color { ColorAnimation { duration: Theme.motion } }
                }
            }

            ShellButton {
                text: "×"
                onClicked: shell.setAppearance({overview: false})
            }
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: Qt.rgba(Theme.starlight.r, Theme.starlight.g, Theme.starlight.b, 0.12)
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: dashboard.tab === 0

            RowLayout {
                anchors.fill: parent
                spacing: 14

                Rectangle {
                    Layout.preferredWidth: 560
                    Layout.fillHeight: true
                    radius: 24
                    clip: true
                    color: Theme.background
                    border.width: 1
                    border.color: Qt.rgba(Theme.moon.r, Theme.moon.g, Theme.moon.b, 0.28)

                    Image {
                        anchors.fill: parent
                        source: shell.state.wallpaperImage || ""
                        fillMode: Image.PreserveAspectCrop
                        asynchronous: true
                        sourceSize: Qt.size(1200, 800)
                    }

                    Rectangle {
                        anchors.fill: parent
                        color: Qt.rgba(0.02, 0.04, 0.10, 0.36)
                    }

                    Column {
                        anchors.left: parent.left
                        anchors.bottom: parent.bottom
                        anchors.margins: 24
                        spacing: 4
                        Text {
                            text: "LunaDash"
                            color: Theme.moon
                            font.family: Theme.font
                            font.pixelSize: 34
                            font.weight: Font.DemiBold
                        }
                        Text {
                            text: shell.tr("A modern Wayland desktop")
                            color: Theme.text
                            font.family: Theme.font
                            font.pixelSize: 13
                        }
                        Text {
                            text: "WAYLAND  ·  " + dashboard.versionText
                            color: Theme.starlight
                            font.family: Theme.font
                            font.pixelSize: 11
                        }
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: 12

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 158
                        radius: 22
                        color: Qt.rgba(Theme.surface.r, Theme.surface.g, Theme.surface.b, 0.82)
                        border.width: 1
                        border.color: Qt.rgba(Theme.starlight.r, Theme.starlight.g, Theme.starlight.b, 0.16)
                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 20
                            ColumnLayout {
                                Layout.fillWidth: true
                                Text { text: dashboard.time; color: Theme.moon; font.family: Theme.font; font.pixelSize: 46; font.weight: Font.Light }
                                Text { text: dashboard.date; color: Theme.muted; font.family: Theme.font; font.pixelSize: 12 }
                            }
                            ColumnLayout {
                                Layout.alignment: Qt.AlignVCenter
                                Text { text: dashboard.weatherTemperature; color: Theme.text; font.family: Theme.font; font.pixelSize: 34; font.weight: Font.Light }
                                Text { text: dashboard.weatherCondition; color: Theme.muted; font.family: Theme.font; font.pixelSize: 11; elide: Text.ElideRight; Layout.maximumWidth: 170 }
                            }
                        }
                    }

                    GridLayout {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        columns: 2
                        columnSpacing: 12
                        rowSpacing: 12

                        Repeater {
                            model: [
                                ["CPU", Math.round(dashboard.stats.cpuPercent || 0) + "%", "monitor"],
                                [shell.tr("Memory"), Math.round(dashboard.stats.memoryPercent || 0) + "%", "modules"],
                                [shell.tr("Network"), dashboard.network.connected ? shell.tr("Connected") : shell.tr("Disconnected"), "network"],
                                [shell.tr("Weather"), dashboard.weatherTemperature, "weather"]
                            ]
                            Rectangle {
                                required property var modelData
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                radius: 18
                                color: Qt.rgba(Theme.surface.r, Theme.surface.g, Theme.surface.b, 0.72)
                                border.width: 1
                                border.color: Qt.rgba(Theme.starlight.r, Theme.starlight.g, Theme.starlight.b, 0.12)
                                ColumnLayout {
                                    anchors.fill: parent
                                    anchors.margins: 16
                                    RowLayout {
                                        Layout.fillWidth: true
                                        LineIcon { width: 18; height: 18; name: modelData[2]; ink: Theme.moon }
                                        Text { Layout.fillWidth: true; text: modelData[0]; color: Theme.muted; font.family: Theme.font; font.pixelSize: 11 }
                                    }
                                    Item { Layout.fillHeight: true }
                                    Text { text: modelData[1]; color: Theme.text; font.family: Theme.font; font.pixelSize: 24; font.weight: Font.DemiBold }
                                }
                            }
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8
                        ShellButton { Layout.fillWidth: true; iconName: "settings"; text: shell.tr("Settings"); onClicked: { shell.setAppearance({overview:false}); shell.settingsOpen = true } }
                        ShellButton { Layout.fillWidth: true; iconName: "files"; text: shell.tr("Files"); onClicked: shell.launch("files") }
                    }
                }
            }
        }

        MediaCard {
            visible: dashboard.tab === 1
            Layout.fillWidth: true
            Layout.fillHeight: true
            shell: dashboard.shell
        }

        GridLayout {
            visible: dashboard.tab === 2
            Layout.fillWidth: true
            Layout.fillHeight: true
            columns: 2
            columnSpacing: 12
            rowSpacing: 12

            Repeater {
                model: [
                    ["CPU", Math.round(dashboard.stats.cpuPercent || 0) + "%", dashboard.stats.cpuModel || "CPU", Math.max(0, Math.min(100, Number(dashboard.stats.cpuPercent || 0)))],
                    ["GPU", Math.round(dashboard.stats.gpuPercent || 0) + "%", dashboard.stats.gpuModel || "GPU", Math.max(0, Math.min(100, Number(dashboard.stats.gpuPercent || 0)))],
                    [shell.tr("Memory"), Math.round(dashboard.stats.memoryPercent || 0) + "%", Number(dashboard.stats.memoryUsed || 0).toFixed(1) + " GiB", Math.max(0, Math.min(100, Number(dashboard.stats.memoryPercent || 0)))],
                    [shell.tr("Storage"), Number(dashboard.stats.diskUsed || 0).toFixed(1) + " GiB", dashboard.stats.diskDevice || "Disk", Math.max(0, Math.min(100, Number(dashboard.stats.diskPercent || 0)))]
                ]
                Rectangle {
                    id: performanceCard
                    required property var modelData
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: 22
                    scale: performanceHover.hovered ? 1.012 : 1
                    color: performanceHover.hovered
                        ? Theme.surfaceElevated
                        : Qt.rgba(Theme.surface.r, Theme.surface.g, Theme.surface.b, 0.76)
                    border.width: performanceHover.hovered ? 1.5 : 1
                    border.color: performanceHover.hovered
                        ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.36)
                        : Qt.rgba(Theme.starlight.r, Theme.starlight.g, Theme.starlight.b, 0.14)

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 20
                        spacing: 8
                        Text { text: modelData[0]; color: Theme.moon; font.family: Theme.font; font.pixelSize: 18; font.weight: Font.DemiBold }
                        Text { text: modelData[2]; color: Theme.muted; font.family: Theme.font; Layout.fillWidth: true; elide: Text.ElideRight }
                        Item { Layout.fillHeight: true }
                        Text { text: modelData[1]; color: Theme.text; font.family: Theme.font; font.pixelSize: 38; font.weight: Font.Light }
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 6
                            radius: 3
                            color: Theme.track
                            clip: true
                            Rectangle {
                                width: parent.width * Number(modelData[3] || 0) / 100
                                height: parent.height
                                radius: parent.radius
                                color: Theme.accent
                                Behavior on width { NumberAnimation { duration: Theme.motion; easing.type: Easing.OutCubic } }
                            }
                        }
                    }

                    HoverHandler { id: performanceHover }
                    Behavior on scale { NumberAnimation { duration: Theme.motionFast; easing.type: Easing.OutCubic } }
                    Behavior on color { ColorAnimation { duration: Theme.motionFast } }
                    Behavior on border.color { ColorAnimation { duration: Theme.motionFast } }
                }
            }
        }

        Rectangle {
            visible: dashboard.tab === 3
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: 24
            color: Qt.rgba(Theme.surface.r, Theme.surface.g, Theme.surface.b, 0.76)
            border.width: 1
            border.color: Qt.rgba(Theme.starlight.r, Theme.starlight.g, Theme.starlight.b, 0.14)
            ColumnLayout {
                anchors.centerIn: parent
                spacing: 12
                LineIcon { Layout.alignment: Qt.AlignHCenter; width: 60; height: 60; name: "weather"; ink: Theme.moon }
                Text { Layout.alignment: Qt.AlignHCenter; text: dashboard.weatherTemperature; color: Theme.text; font.family: Theme.font; font.pixelSize: 42; font.weight: Font.Light }
                Text { Layout.alignment: Qt.AlignHCenter; text: dashboard.weatherLocation; color: Theme.moon; font.family: Theme.font; font.pixelSize: 18; font.weight: Font.DemiBold }
                Text { Layout.alignment: Qt.AlignHCenter; text: dashboard.weatherCondition; color: Theme.muted; font.family: Theme.font; font.pixelSize: 12 }
            }
        }
    }

    Timer {
        interval: 1000
        repeat: true
        running: true
        triggeredOnStart: true
        onTriggered: {
            const now = new Date()
            dashboard.time = Qt.formatDateTime(now, Theme.clock24Hour ? "HH:mm" : "h:mm AP")
            dashboard.date = Qt.formatDateTime(now, "yyyy/MM/dd dddd")
        }
    }
}
