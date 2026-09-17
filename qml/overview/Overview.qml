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
    property real reveal: opened ? 1 : 0

    readonly property var network: shell.state.network || ({})
    readonly property var weather: shell.state.weather || ({})
    readonly property var updateInfo: shell.state.update || ({})
    readonly property string versionText: updateInfo.version || updateInfo.currentVersion || "0.1.0"
    readonly property string weatherLocation: weather.location || weather.city || shell.tr("Weather")
    readonly property string weatherCondition: weather.condition || weather.summary || shell.tr("Weather service is not configured")
    readonly property string weatherTemperature: weather.temperature !== undefined ? String(weather.temperature) + "°" : "—"

    anchors.top: true
    anchors.horizontalCenter: true
    margins.top: Theme.barHeight + moduleMargin + 8
    implicitWidth: moduleWidth(1060)
    implicitHeight: moduleHeight(650)
    exclusionMode: ExclusionMode.Ignore
    WlrLayershell.layer: WlrLayer.Top
    WlrLayershell.namespace: "lunadash-overview"
    color: "transparent"

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

        RowLayout {
            visible: dashboard.tab === 0
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 12

            ColumnLayout {
                Layout.preferredWidth: 590
                Layout.fillHeight: true
                spacing: 12

                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: 24
                    clip: true
                    color: Theme.background
                    border.width: 1
                    border.color: Qt.rgba(Theme.moon.r, Theme.moon.g, Theme.moon.b, 0.24)

                    Image {
                        anchors.fill: parent
                        source: shell.wallpaperOverride.length ? shell.wallpaperOverride : (shell.state.wallpaperImage || "")
                        fillMode: Image.PreserveAspectCrop
                        asynchronous: true
                        sourceSize: Qt.size(1000, 700)
                    }

                    Rectangle {
                        anchors.fill: parent
                        color: Qt.rgba(0.02, 0.03, 0.09, 0.30)
                    }

                    Rectangle {
                        width: 260
                        height: 260
                        radius: 130
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.margins: -70
                        color: Qt.rgba(Theme.moon.r, Theme.moon.g, Theme.moon.b, 0.08)
                        border.width: 1
                        border.color: Qt.rgba(Theme.moon.r, Theme.moon.g, Theme.moon.b, 0.20)
                    }

                    Column {
                        anchors.left: parent.left
                        anchors.bottom: parent.bottom
                        anchors.margins: 24
                        spacing: 5
                        Text {
                            text: shell.tr("Welcome back")
                            color: Theme.moon
                            font.family: Theme.font
                            font.pixelSize: 13
                            font.letterSpacing: 1.2
                        }
                        Text {
                            text: dashboard.stats.displayName || dashboard.stats.user || "LunaDash"
                            color: "white"
                            font.family: Theme.font
                            font.pixelSize: 34
                            font.weight: Font.DemiBold
                        }
                        Text {
                            text: "LunaDash · Wayland · " + dashboard.versionText
                            color: Qt.rgba(1, 1, 1, 0.72)
                            font.family: Theme.font
                            font.pixelSize: 11
                        }
                    }

                    Column {
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        anchors.margins: 24
                        spacing: 2
                        Text {
                            anchors.right: parent.right
                            text: dashboard.time
                            color: Theme.moon
                            font.family: Theme.font
                            font.pixelSize: 48
                            font.weight: Font.Light
                        }
                        Text {
                            anchors.right: parent.right
                            text: dashboard.date
                            color: Qt.rgba(1, 1, 1, 0.68)
                            font.family: Theme.font
                            font.pixelSize: 11
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 132
                    spacing: 12

                    Repeater {
                        model: [
                            ["CPU", Math.round(dashboard.stats.cpuPercent || 0) + "%", "monitor"],
                            [shell.tr("Memory"), Math.round(dashboard.stats.memoryPercent || 0) + "%", "devices"],
                            [shell.tr("Network"), dashboard.network.connected ? shell.tr("Online") : shell.tr("Offline"), "network"]
                        ]

                        Rectangle {
                            required property var modelData
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            radius: 18
                            color: Qt.rgba(Theme.surface.r, Theme.surface.g, Theme.surface.b, 0.82)
                            border.width: 1
                            border.color: Qt.rgba(Theme.starlight.r, Theme.starlight.g, Theme.starlight.b, 0.15)
                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: 15
                                RowLayout {
                                    Layout.fillWidth: true
                                    LineIcon { width: 17; height: 17; name: modelData[2]; ink: Theme.moon }
                                    Text { Layout.fillWidth: true; text: modelData[0]; color: Theme.muted; font.family: Theme.font; font.pixelSize: 11 }
                                }
                                Item { Layout.fillHeight: true }
                                Text { text: modelData[1]; color: Theme.text; font.family: Theme.font; font.pixelSize: 26; font.weight: Font.DemiBold }
                            }
                        }
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 12

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 200
                    radius: 22
                    color: Qt.rgba(Theme.surface.r, Theme.surface.g, Theme.surface.b, 0.80)
                    border.width: 1
                    border.color: Qt.rgba(Theme.moon.r, Theme.moon.g, Theme.moon.b, 0.18)

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 18
                        RowLayout {
                            Layout.fillWidth: true
                            LineIcon { width: 22; height: 22; name: "weather"; ink: Theme.moon }
                            Text { Layout.fillWidth: true; text: dashboard.weatherLocation; color: Theme.text; font.family: Theme.font; font.pixelSize: 15; font.weight: Font.DemiBold }
                            Text { text: dashboard.weatherTemperature; color: Theme.moon; font.family: Theme.font; font.pixelSize: 30; font.weight: Font.Light }
                        }
                        Text {
                            Layout.fillWidth: true
                            text: dashboard.weatherCondition
                            color: Theme.muted
                            font.family: Theme.font
                            font.pixelSize: 12
                            wrapMode: Text.WordWrap
                        }
                        Item { Layout.fillHeight: true }
                        RowLayout {
                            Layout.fillWidth: true
                            Text { Layout.fillWidth: true; text: shell.tr("Graphics"); color: Theme.muted; font.family: Theme.font; font.pixelSize: 11 }
                            Text { text: shell.state.graphicsApi || "OpenGL"; color: Theme.text; font.family: Theme.font; font.pixelSize: 11 }
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            Text { Layout.fillWidth: true; text: shell.tr("Kernel"); color: Theme.muted; font.family: Theme.font; font.pixelSize: 11 }
                            Text { text: dashboard.stats.kernel || "—"; color: Theme.text; font.family: Theme.font; font.pixelSize: 11; elide: Text.ElideRight; Layout.maximumWidth: 180 }
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: 22
                    color: Qt.rgba(Theme.surface.r, Theme.surface.g, Theme.surface.b, 0.80)
                    border.width: 1
                    border.color: Qt.rgba(Theme.starlight.r, Theme.starlight.g, Theme.starlight.b, 0.15)

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 18
                        Text { text: shell.tr("Quick actions"); color: Theme.text; font.family: Theme.font; font.pixelSize: 15; font.weight: Font.DemiBold }
                        Text { text: shell.tr("Move through LunaDash without leaving the dashboard."); color: Theme.muted; font.family: Theme.font; font.pixelSize: 11; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                        Item { Layout.fillHeight: true }
                        ShellButton { Layout.fillWidth: true; text: shell.tr("Open settings"); onClicked: { shell.setAppearance({overview:false}); shell.settingsOpen = true } }
                        ShellButton { Layout.fillWidth: true; text: shell.tr("Open files"); onClicked: { shell.setAppearance({overview:false}); shell.launch("files") } }
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
                    ["CPU", Math.round(dashboard.stats.cpuPercent || 0) + "%", dashboard.stats.cpuModel || "CPU", "monitor"],
                    ["GPU", Math.round(dashboard.stats.gpuPercent || 0) + "%", dashboard.stats.gpuModel || "GPU", "display"],
                    [shell.tr("Memory"), Math.round(dashboard.stats.memoryPercent || 0) + "%", Number(dashboard.stats.memoryUsed || 0).toFixed(1) + " GiB", "devices"],
                    [shell.tr("Storage"), Number(dashboard.stats.diskUsed || 0).toFixed(1) + " GiB", dashboard.stats.diskDevice || "Disk", "files"]
                ]

                Rectangle {
                    required property var modelData
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: 22
                    color: Qt.rgba(Theme.surface.r, Theme.surface.g, Theme.surface.b, 0.82)
                    border.width: 1
                    border.color: Qt.rgba(Theme.starlight.r, Theme.starlight.g, Theme.starlight.b, 0.16)

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 20
                        RowLayout {
                            Layout.fillWidth: true
                            LineIcon { width: 20; height: 20; name: modelData[3]; ink: Theme.moon }
                            Text { Layout.fillWidth: true; text: modelData[0]; color: Theme.text; font.family: Theme.font; font.pixelSize: 17; font.weight: Font.DemiBold }
                        }
                        Text { Layout.fillWidth: true; text: modelData[2]; color: Theme.muted; font.family: Theme.font; font.pixelSize: 11; elide: Text.ElideRight }
                        Item { Layout.fillHeight: true }
                        Text { text: modelData[1]; color: Theme.moon; font.family: Theme.font; font.pixelSize: 40; font.weight: Font.Light }
                    }
                }
            }
        }

        Rectangle {
            visible: dashboard.tab === 3
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: 24
            color: Qt.rgba(Theme.surface.r, Theme.surface.g, Theme.surface.b, 0.78)
            border.width: 1
            border.color: Qt.rgba(Theme.moon.r, Theme.moon.g, Theme.moon.b, 0.18)

            RowLayout {
                anchors.fill: parent
                anchors.margins: 26
                spacing: 28

                Rectangle {
                    Layout.preferredWidth: 300
                    Layout.fillHeight: true
                    radius: 24
                    color: Qt.rgba(Theme.background.r, Theme.background.g, Theme.background.b, 0.55)
                    border.width: 1
                    border.color: Qt.rgba(Theme.starlight.r, Theme.starlight.g, Theme.starlight.b, 0.14)

                    ColumnLayout {
                        anchors.centerIn: parent
                        spacing: 10
                        LineIcon { Layout.alignment: Qt.AlignHCenter; width: 72; height: 72; name: "weather"; ink: Theme.moon }
                        Text { Layout.alignment: Qt.AlignHCenter; text: dashboard.weatherTemperature; color: Theme.moon; font.family: Theme.font; font.pixelSize: 54; font.weight: Font.Light }
                        Text { Layout.alignment: Qt.AlignHCenter; text: dashboard.weatherLocation; color: Theme.text; font.family: Theme.font; font.pixelSize: 17; font.weight: Font.DemiBold }
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: 12
                    Text { text: shell.tr("Weather"); color: Theme.text; font.family: Theme.font; font.pixelSize: 28; font.weight: Font.DemiBold }
                    Text { Layout.fillWidth: true; text: dashboard.weatherCondition; color: Theme.muted; font.family: Theme.font; font.pixelSize: 13; wrapMode: Text.WordWrap }
                    Rectangle { Layout.fillWidth: true; height: 1; color: Qt.rgba(Theme.starlight.r, Theme.starlight.g, Theme.starlight.b, 0.12) }
                    Text {
                        Layout.fillWidth: true
                        text: weather.temperature === undefined
                            ? shell.tr("Connect a weather provider to show live forecasts here. This page is intentionally separate from Wi-Fi status.")
                            : shell.tr("Live weather data is available from the shell weather state.")
                        color: Theme.muted
                        font.family: Theme.font
                        font.pixelSize: 12
                        wrapMode: Text.WordWrap
                    }
                    Item { Layout.fillHeight: true }
                }
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
