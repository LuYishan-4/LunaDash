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
    readonly property var audio: shell.state.audio || ({})
    readonly property var weather: shell.state.weather || ({})
    readonly property var updateInfo: shell.state.update || ({})
    readonly property var clients: (shell.state.clients || []).filter(client => client.mapped && !client.desktop)
    readonly property string versionText: updateInfo.currentVersion || "1.0.1a"
    readonly property string weatherLocation: weather.location || weather.city || shell.tr("Weather")
    readonly property string weatherCondition: weather.condition || weather.summary || shell.tr("Weather data unavailable")
    readonly property string weatherTemperature: weather.temperature !== undefined ? String(weather.temperature) + "°" : "—"
    readonly property string networkText: network.connected
        ? (network.primaryConnection || (network.ethernetConnected ? shell.tr("Ethernet") : shell.tr("Connected")))
        : shell.tr("Disconnected")
    readonly property string gpuValue: stats.gpuAvailable === false
        ? "N/A" : Math.round(Number(stats.gpuPercent || 0)) + "%"
    readonly property string audioValue: (audio.output || {}).available === false
        ? "N/A" : Math.round(Number((audio.output || {}).volume || 0)) + "%"

    anchors.top: true
    anchors.left: true
    margins.top: Theme.panelTopInset + moduleMargin + 10
    margins.left: Math.max(moduleMargin, Math.round(((screen ? screen.width : 1440) - implicitWidth) / 2))
    implicitWidth: moduleWidth(1120)
    implicitHeight: moduleHeight(720)
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
        interval: 600
        onTriggered: {
            dashboard.closeArmed = true
            if (!dashboardHover.hovered)
                closeTimer.restart()
        }
    }

    Timer {
        id: closeTimer
        interval: 300
        onTriggered: if (dashboard.opened && !dashboardHover.hovered)
            shell.setAppearance({overview:false})
    }

    onOpenedChanged: {
        closeArmed = false
        if (opened)
            armCloseTimer.restart()
        else {
            armCloseTimer.stop()
            closeTimer.stop()
        }
    }

    Rectangle {
        anchors.fill: parent
        radius: Theme.radiusHero
        color: Theme.surfaceStrong
        border.width: 1
        border.color: Qt.rgba(Theme.starlight.r, Theme.starlight.g, Theme.starlight.b, 0.28)
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 18
        spacing: 12

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 1
                Text {
                    text: shell.tr("Dashboard")
                    color: Theme.text
                    font.family: Theme.font
                    font.pixelSize: 21
                    font.weight: Font.DemiBold
                }
                Text {
                    text: dashboard.date + "  ·  LunaDash " + dashboard.versionText
                    color: Theme.muted
                    font.family: Theme.font
                    font.pixelSize: 10
                }
            }

            Rectangle {
                implicitWidth: navRow.implicitWidth + 10
                implicitHeight: 42
                radius: height / 2
                color: Theme.surfaceGlass
                border.width: 1
                border.color: Theme.hairline

                Row {
                    id: navRow
                    anchors.centerIn: parent
                    spacing: 2
                    Repeater {
                        model: [
                            ["Home", "apps"],
                            ["Media", "sound"],
                            ["Performance", "monitor"],
                            ["Weather", "weather"]
                        ]
                        Item {
                            required property var modelData
                            required property int index
                            width: navContent.implicitWidth + 20
                            height: 34
                            Rectangle {
                                anchors.fill: parent
                                radius: height / 2
                                color: dashboard.tab === index
                                    ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.19)
                                    : navMouse.containsMouse
                                        ? Qt.rgba(Theme.starlight.r, Theme.starlight.g, Theme.starlight.b, 0.08)
                                        : "transparent"
                                border.width: dashboard.tab === index ? 1 : 0
                                border.color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.44)
                            }
                            Row {
                                id: navContent
                                anchors.centerIn: parent
                                spacing: 6
                                LineIcon {
                                    width: 15
                                    height: 15
                                    name: modelData[1]
                                    ink: dashboard.tab === index ? Theme.accent : Theme.muted
                                }
                                Text {
                                    text: shell.tr(modelData[0])
                                    color: dashboard.tab === index ? Theme.text : Theme.muted
                                    font.family: Theme.font
                                    font.pixelSize: 10
                                    font.weight: dashboard.tab === index ? Font.DemiBold : Font.Normal
                                }
                            }
                            MouseArea {
                                id: navMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: dashboard.tab = index
                            }
                        }
                    }
                }
            }

            ShellButton {
                text: "×"
                quiet: true
                onClicked: shell.setAppearance({overview:false})
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            ScrollView {
                id: homeScroll
                anchors.fill: parent
                visible: dashboard.tab === 0
                clip: true
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                ColumnLayout {
                    width: homeScroll.availableWidth
                    spacing: 12

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 12

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 210
                            radius: 26
                            clip: true
                            color: Theme.background
                            border.width: 1
                            border.color: Theme.hairline

                            Image {
                                anchors.fill: parent
                                source: shell.state.wallpaperImage || ""
                                fillMode: Image.PreserveAspectCrop
                                asynchronous: true
                                sourceSize: Qt.size(900, 500)
                            }
                            Rectangle {
                                anchors.fill: parent
                                gradient: Gradient {
                                    GradientStop { position: 0; color: Qt.rgba(0.02, 0.03, 0.09, 0.18) }
                                    GradientStop { position: 1; color: Qt.rgba(0.02, 0.03, 0.09, 0.78) }
                                }
                            }

                            Row {
                                anchors.left: parent.left
                                anchors.bottom: parent.bottom
                                anchors.margins: 20
                                spacing: 14

                                Rectangle {
                                    width: 58
                                    height: 58
                                    radius: 29
                                    color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.18)
                                    border.width: 1
                                    border.color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.44)
                                    clip: true
                                    Image {
                                        anchors.fill: parent
                                        source: dashboard.stats.avatar || ""
                                        fillMode: Image.PreserveAspectCrop
                                        visible: source.toString().length > 0 && status !== Image.Error
                                    }
                                    LunaDashLogo {
                                        anchors.centerIn: parent
                                        width: 34
                                        height: 34
                                        visible: !dashboard.stats.avatar
                                        animated: false
                                    }
                                }

                                Column {
                                    anchors.verticalCenter: parent.verticalCenter
                                    spacing: 2
                                    Text {
                                        text: dashboard.stats.displayName || dashboard.stats.user || shell.tr("Welcome")
                                        color: Theme.text
                                        font.family: Theme.font
                                        font.pixelSize: 20
                                        font.weight: Font.DemiBold
                                    }
                                    Text {
                                        text: dashboard.stats.host || shell.tr("Local session")
                                        color: Theme.muted
                                        font.family: Theme.font
                                        font.pixelSize: 10
                                    }
                                }
                            }

                            Column {
                                anchors.right: parent.right
                                anchors.top: parent.top
                                anchors.margins: 20
                                spacing: -4
                                Text {
                                    anchors.right: parent.right
                                    text: dashboard.time
                                    color: Theme.moon
                                    font.family: Theme.font
                                    font.pixelSize: 42
                                    font.weight: Font.Light
                                }
                                Text {
                                    anchors.right: parent.right
                                    text: dashboard.weatherTemperature + "  " + dashboard.weatherCondition
                                    color: Theme.starlight
                                    font.family: Theme.font
                                    font.pixelSize: 10
                                }
                            }
                        }

                        Rectangle {
                            Layout.preferredWidth: 320
                            Layout.preferredHeight: 210
                            radius: 26
                            color: Theme.surfaceGlass
                            border.width: 1
                            border.color: Theme.hairline

                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: 18
                                spacing: 8
                                Text {
                                    text: shell.tr("Quick actions")
                                    color: Theme.text
                                    font.family: Theme.font
                                    font.pixelSize: 15
                                    font.weight: Font.DemiBold
                                }
                                GridLayout {
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    columns: 2
                                    columnSpacing: 7
                                    rowSpacing: 7
                                    Repeater {
                                        model: [
                                            ["Settings", "settings", "settings"],
                                            ["Files", "files", "files"],
                                            ["Terminal", "console", "terminal"],
                                            ["Plugins", "apps", "plugins"]
                                        ]
                                        ShellButton {
                                            required property var modelData
                                            Layout.fillWidth: true
                                            Layout.fillHeight: true
                                            iconName: modelData[1]
                                            text: shell.tr(modelData[0])
                                            onClicked: {
                                                if (modelData[2] === "settings") {
                                                    shell.setAppearance({overview:false})
                                                    shell.settingsOpen = true
                                                } else if (modelData[2] === "plugins") {
                                                    shell.setAppearance({overview:false})
                                                    shell.openSettingsPage("plugins")
                                                } else {
                                                    shell.launch(modelData[2])
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }

                    GridLayout {
                        Layout.fillWidth: true
                        columns: dashboard.width >= 900 ? 4 : 2
                        columnSpacing: 10
                        rowSpacing: 10

                        Repeater {
                            model: [
                                ["CPU", Math.round(Number(dashboard.stats.cpuPercent || 0)) + "%", dashboard.stats.cpuModel || "CPU", "monitor"],
                                ["GPU", dashboard.gpuValue, dashboard.stats.gpuModel || dashboard.stats.gpuDriver || shell.tr("Unavailable"), "monitor"],
                                [shell.tr("Memory"), Math.round(Number(dashboard.stats.memoryPercent || 0)) + "%", Number(dashboard.stats.memoryUsed || 0).toFixed(1) + " GiB", "modules"],
                                [shell.tr("Storage"), Math.round(Number(dashboard.stats.diskPercent || 0)) + "%", dashboard.stats.diskDevice || shell.tr("Home storage"), "files"]
                            ]
                            Rectangle {
                                required property var modelData
                                Layout.fillWidth: true
                                Layout.preferredHeight: 116
                                radius: 20
                                color: Theme.surfaceGlass
                                border.width: 1
                                border.color: Theme.hairline

                                ColumnLayout {
                                    anchors.fill: parent
                                    anchors.margins: 14
                                    spacing: 3
                                    RowLayout {
                                        Layout.fillWidth: true
                                        LineIcon { width: 17; height: 17; name: modelData[3]; ink: Theme.accent }
                                        Text { Layout.fillWidth: true; text: modelData[0]; color: Theme.muted; font.family: Theme.font; font.pixelSize: 10 }
                                    }
                                    Text { text: modelData[1]; color: Theme.text; font.family: Theme.font; font.pixelSize: 25; font.weight: Font.DemiBold }
                                    Text { Layout.fillWidth: true; text: modelData[2]; color: Theme.muted; font.family: Theme.font; font.pixelSize: 9; elide: Text.ElideRight }
                                }
                            }
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 12

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 170
                            radius: 22
                            color: Theme.surfaceGlass
                            border.width: 1
                            border.color: Theme.hairline

                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: 16
                                spacing: 8
                                RowLayout {
                                    Layout.fillWidth: true
                                    Text { Layout.fillWidth: true; text: shell.tr("Active windows"); color: Theme.text; font.family: Theme.font; font.pixelSize: 14; font.weight: Font.DemiBold }
                                    Text { text: String(dashboard.clients.length); color: Theme.accent; font.family: Theme.font; font.pixelSize: 11 }
                                }
                                Row {
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    spacing: 8
                                    Repeater {
                                        model: dashboard.clients.slice(0, 6)
                                        Item {
                                            required property var modelData
                                            width: 86
                                            height: 104
                                            Rectangle {
                                                anchors.fill: parent
                                                radius: 16
                                                color: windowMouse.containsMouse ? Theme.surfaceElevated : Theme.control
                                                border.width: modelData.focused ? 1.5 : 1
                                                border.color: modelData.focused ? Theme.accent : Theme.hairline
                                            }
                                            ApplicationIcon {
                                                shell: dashboard.shell
                                                anchors.horizontalCenter: parent.horizontalCenter
                                                anchors.top: parent.top
                                                anchors.topMargin: 14
                                                width: 30
                                                height: 30
                                                iconName: String(modelData.icon || "")
                                                appId: String(modelData.appId || "")
                                                title: String(modelData.title || "")
                                            }
                                            Text {
                                                anchors.left: parent.left
                                                anchors.right: parent.right
                                                anchors.bottom: parent.bottom
                                                anchors.margins: 8
                                                text: modelData.title || modelData.appId || shell.tr("Window")
                                                color: Theme.text
                                                font.family: Theme.font
                                                font.pixelSize: 9
                                                horizontalAlignment: Text.AlignHCenter
                                                elide: Text.ElideRight
                                            }
                                            MouseArea {
                                                id: windowMouse
                                                anchors.fill: parent
                                                hoverEnabled: true
                                                cursorShape: Qt.PointingHandCursor
                                                onClicked: dashboard.shell.command("activate-window", modelData.id)
                                            }
                                        }
                                    }
                                    Text {
                                        visible: dashboard.clients.length === 0
                                        text: shell.tr("No application windows are open.")
                                        color: Theme.muted
                                        font.family: Theme.font
                                        font.pixelSize: 11
                                    }
                                }
                            }
                        }

                        Rectangle {
                            Layout.preferredWidth: 330
                            Layout.preferredHeight: 170
                            radius: 22
                            color: Theme.surfaceGlass
                            border.width: 1
                            border.color: Theme.hairline

                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: 16
                                spacing: 7
                                Text { text: shell.tr("Desktop status"); color: Theme.text; font.family: Theme.font; font.pixelSize: 14; font.weight: Font.DemiBold }

                                RowLayout {
                                    Layout.fillWidth: true
                                    LineIcon { width: 16; height: 16; name: "network"; ink: dashboard.network.connected ? Theme.success : Theme.warning }
                                    Text { Layout.fillWidth: true; text: dashboard.networkText; color: Theme.text; font.family: Theme.font; font.pixelSize: 10; elide: Text.ElideRight }
                                }
                                RowLayout {
                                    Layout.fillWidth: true
                                    LineIcon { width: 16; height: 16; name: "sound"; ink: Theme.accent }
                                    Text { Layout.fillWidth: true; text: shell.tr("Output volume"); color: Theme.muted; font.family: Theme.font; font.pixelSize: 10 }
                                    Text { text: dashboard.audioValue; color: Theme.text; font.family: Theme.font; font.pixelSize: 10 }
                                }
                                RowLayout {
                                    Layout.fillWidth: true
                                    LineIcon { width: 16; height: 16; name: "power"; ink: Theme.accent }
                                    Text { Layout.fillWidth: true; text: shell.tr("Battery"); color: Theme.muted; font.family: Theme.font; font.pixelSize: 10 }
                                    Text {
                                        text: Number(dashboard.stats.batteryPercent ?? -1) < 0
                                            ? shell.tr("Desktop")
                                            : Math.round(Number(dashboard.stats.batteryPercent)) + "%"
                                        color: Theme.text
                                        font.family: Theme.font
                                        font.pixelSize: 10
                                    }
                                }
                                Item { Layout.fillHeight: true }
                                Text {
                                    Layout.fillWidth: true
                                    text: dashboard.updateInfo.status === "available"
                                        ? shell.tr("A LunaDash update is available.")
                                        : shell.tr("System services are connected.")
                                    color: dashboard.updateInfo.status === "available" ? Theme.warning : Theme.muted
                                    font.family: Theme.font
                                    font.pixelSize: 9
                                    wrapMode: Text.WordWrap
                                }
                            }
                        }
                    }
                }
            }

            MediaCard {
                anchors.fill: parent
                visible: dashboard.tab === 1
                shell: dashboard.shell
            }

            ScrollView {
                anchors.fill: parent
                visible: dashboard.tab === 2
                clip: true
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                GridLayout {
                    width: parent.availableWidth
                    columns: dashboard.width >= 760 ? 2 : 1
                    columnSpacing: 12
                    rowSpacing: 12

                    Repeater {
                        model: [
                            ["CPU", Math.round(Number(dashboard.stats.cpuPercent || 0)) + "%", dashboard.stats.cpuModel || "CPU", Math.max(0, Math.min(100, Number(dashboard.stats.cpuPercent || 0))), "monitor"],
                            ["GPU", dashboard.gpuValue, dashboard.stats.gpuModel || dashboard.stats.gpuDriver || shell.tr("GPU telemetry unavailable"), dashboard.stats.gpuAvailable === false ? 0 : Math.max(0, Math.min(100, Number(dashboard.stats.gpuPercent || 0))), "monitor"],
                            [shell.tr("Memory"), Math.round(Number(dashboard.stats.memoryPercent || 0)) + "%", Number(dashboard.stats.memoryUsed || 0).toFixed(1) + " / " + Number(dashboard.stats.memoryTotal || 0).toFixed(1) + " GiB", Math.max(0, Math.min(100, Number(dashboard.stats.memoryPercent || 0))), "modules"],
                            [shell.tr("Storage"), Math.round(Number(dashboard.stats.diskPercent || 0)) + "%", Number(dashboard.stats.diskUsed || 0).toFixed(1) + " / " + Number(dashboard.stats.diskTotal || 0).toFixed(1) + " GiB", Math.max(0, Math.min(100, Number(dashboard.stats.diskPercent || 0))), "files"]
                        ]

                        Rectangle {
                            id: performanceCard
                            required property var modelData
                            Layout.fillWidth: true
                            Layout.preferredHeight: 220
                            radius: 24
                            color: performanceHover.hovered ? Theme.surfaceElevated : Theme.surfaceGlass
                            border.width: 1
                            border.color: performanceHover.hovered
                                ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.42)
                                : Theme.hairline

                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: 20
                                spacing: 7
                                RowLayout {
                                    Layout.fillWidth: true
                                    LineIcon { width: 20; height: 20; name: modelData[4]; ink: Theme.accent }
                                    Text { Layout.fillWidth: true; text: modelData[0]; color: Theme.text; font.family: Theme.font; font.pixelSize: 17; font.weight: Font.DemiBold }
                                    Text { text: modelData[1]; color: Theme.moon; font.family: Theme.font; font.pixelSize: 28; font.weight: Font.Light }
                                }
                                Text { Layout.fillWidth: true; text: modelData[2]; color: Theme.muted; font.family: Theme.font; font.pixelSize: 10; elide: Text.ElideRight }
                                Item { Layout.fillHeight: true }
                                Rectangle {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 10
                                    radius: height / 2
                                    color: Theme.track
                                    clip: true
                                    Rectangle {
                                        width: parent.width * Number(modelData[3] || 0) / 100
                                        height: parent.height
                                        radius: parent.radius
                                        gradient: Gradient {
                                            orientation: Gradient.Horizontal
                                            GradientStop { position: 0; color: Theme.secondaryAccent }
                                            GradientStop { position: 1; color: Theme.accent }
                                        }
                                        Behavior on width { NumberAnimation { duration: Theme.motion; easing.type: Easing.OutCubic } }
                                    }
                                }
                                Text {
                                    text: modelData[0] === "GPU" && dashboard.stats.gpuAvailable === false
                                        ? shell.tr("No supported GPU telemetry source was found; LunaDash will not display a fake 0%.")
                                        : shell.tr("Live telemetry refreshes automatically.")
                                    color: Theme.muted
                                    font.family: Theme.font
                                    font.pixelSize: 9
                                    wrapMode: Text.WordWrap
                                    Layout.fillWidth: true
                                }
                            }
                            HoverHandler { id: performanceHover }
                        }
                    }
                }
            }

            Rectangle {
                anchors.fill: parent
                visible: dashboard.tab === 3
                radius: 26
                color: Theme.surfaceGlass
                border.width: 1
                border.color: Theme.hairline

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 28
                    spacing: 28
                    Rectangle {
                        Layout.preferredWidth: 210
                        Layout.preferredHeight: 210
                        radius: 105
                        color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.09)
                        border.width: 1
                        border.color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.28)
                        LineIcon { anchors.centerIn: parent; width: 90; height: 90; name: "weather"; ink: Theme.moon }
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 9
                        Text { text: dashboard.weatherTemperature; color: Theme.text; font.family: Theme.font; font.pixelSize: 56; font.weight: Font.Light }
                        Text { text: dashboard.weatherLocation; color: Theme.moon; font.family: Theme.font; font.pixelSize: 22; font.weight: Font.DemiBold }
                        Text {
                            Layout.fillWidth: true
                            text: dashboard.weatherCondition
                            color: Theme.muted
                            font.family: Theme.font
                            font.pixelSize: 12
                            wrapMode: Text.WordWrap
                        }
                        Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: Theme.hairline }
                        Text {
                            Layout.fillWidth: true
                            text: weather.temperature === undefined
                                ? shell.tr("Weather is optional. The dashboard keeps this page useful even when no weather provider is configured.")
                                : shell.tr("Weather data is available for this session.")
                            color: Theme.muted
                            font.family: Theme.font
                            font.pixelSize: 10
                            wrapMode: Text.WordWrap
                        }
                    }
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
