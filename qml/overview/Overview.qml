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
    readonly property var clients: (shell.state.clients || [])
        .filter(client => client.mapped && !client.desktop)
    readonly property string versionText: updateInfo.currentVersion || "1.0.1a"
    readonly property string weatherLocation:
        weather.location || weather.city || shell.tr("Weather")
    readonly property string weatherCondition:
        weather.condition || weather.summary || shell.tr("Weather data unavailable")
    readonly property string weatherTemperature:
        weather.temperature !== undefined ? String(weather.temperature) + "°" : "—"
    readonly property string networkText: network.connected
        ? (network.primaryConnection
            || (network.ethernetConnected ? shell.tr("Ethernet") : shell.tr("Connected")))
        : shell.tr("Disconnected")
    readonly property string gpuValue: stats.gpuAvailable === false
        ? "N/A" : Math.round(Number(stats.gpuPercent || 0)) + "%"
    readonly property string audioValue: (audio.output || {}).available === false
        ? "N/A" : Math.round(Number((audio.output || {}).volume || 0)) + "%"

    readonly property var metrics: [
        {title:"CPU", value:Math.round(Number(stats.cpuPercent || 0)) + "%",
         detail:stats.cpuModel || "CPU", icon:"monitor",
         progress:Math.max(0, Math.min(100, Number(stats.cpuPercent || 0)))},
        {title:"GPU", value:gpuValue,
         detail:stats.gpuModel || stats.gpuDriver || shell.tr("Unavailable"),
         icon:"monitor",
         progress:stats.gpuAvailable === false ? 0
             : Math.max(0, Math.min(100, Number(stats.gpuPercent || 0)))},
        {title:shell.tr("Memory"),
         value:Math.round(Number(stats.memoryPercent || 0)) + "%",
         detail:Number(stats.memoryUsed || 0).toFixed(1) + " / "
             + Number(stats.memoryTotal || 0).toFixed(1) + " GiB",
         icon:"modules",
         progress:Math.max(0, Math.min(100, Number(stats.memoryPercent || 0)))},
        {title:shell.tr("Storage"),
         value:Math.round(Number(stats.diskPercent || 0)) + "%",
         detail:stats.diskDevice || shell.tr("Home storage"),
         icon:"files",
         progress:Math.max(0, Math.min(100, Number(stats.diskPercent || 0)))}
    ]

    anchors.top: true
    anchors.left: true
    margins.top: Theme.panelTopInset + moduleMargin + 10
    margins.left: Math.max(moduleMargin,
        Math.round(((screen ? screen.width : 1440) - implicitWidth) / 2))
    implicitWidth: moduleWidth(1180)
    implicitHeight: moduleHeight(760)
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
        border.color: Qt.rgba(Theme.starlight.r, Theme.starlight.g,
                              Theme.starlight.b, 0.28)
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 18
        spacing: 12

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 58
            radius: 20
            color: Theme.surfaceGlass
            border.width: 1
            border.color: Theme.hairline

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 18
                anchors.rightMargin: 10
                spacing: 10

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    spacing: 0
                    Text {
                        Layout.fillWidth: true
                        text: shell.tr("Dashboard")
                        color: Theme.text
                        font.family: Theme.font
                        font.pixelSize: 20
                        font.weight: Font.DemiBold
                        elide: Text.ElideRight
                    }
                    Text {
                        Layout.fillWidth: true
                        text: dashboard.date + "  ·  LunaDash " + dashboard.versionText
                        color: Theme.muted
                        font.family: Theme.font
                        font.pixelSize: 10
                        elide: Text.ElideRight
                    }
                }

                Rectangle {
                    implicitWidth: tabRow.implicitWidth + 10
                    implicitHeight: 38
                    radius: height / 2
                    color: Theme.control
                    border.width: 1
                    border.color: Theme.hairline

                    Row {
                        id: tabRow
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
                                width: tabContent.implicitWidth + 18
                                height: 30
                                Rectangle {
                                    anchors.fill: parent
                                    radius: height / 2
                                    color: dashboard.tab === index
                                        ? Qt.rgba(Theme.accent.r, Theme.accent.g,
                                                  Theme.accent.b, 0.20)
                                        : tabMouse.containsMouse
                                            ? Theme.controlHover : "transparent"
                                    border.width: dashboard.tab === index ? 1 : 0
                                    border.color: Qt.rgba(Theme.accent.r, Theme.accent.g,
                                                          Theme.accent.b, 0.42)
                                }
                                Row {
                                    id: tabContent
                                    anchors.centerIn: parent
                                    spacing: 5
                                    LineIcon {
                                        width: 14
                                        height: 14
                                        name: modelData[1]
                                        ink: dashboard.tab === index
                                            ? Theme.accent : Theme.muted
                                    }
                                    Text {
                                        text: shell.tr(modelData[0])
                                        color: dashboard.tab === index
                                            ? Theme.text : Theme.muted
                                        font.family: Theme.font
                                        font.pixelSize: 9
                                    }
                                }
                                MouseArea {
                                    id: tabMouse
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
        }

        StackLayout {
            id: pages
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: dashboard.tab

            Item {
                GridLayout {
                    anchors.fill: parent
                    columns: 12
                    columnSpacing: 10
                    rowSpacing: 10

                    Rectangle {
                        Layout.columnSpan: 8
                        Layout.fillWidth: true
                        Layout.preferredHeight: 188
                        Layout.minimumWidth: 0
                        radius: 24
                        color: Theme.background
                        border.width: 1
                        border.color: Theme.hairline
                        clip: true

                        Image {
                            anchors.fill: parent
                            source: shell.state.wallpaperImage || ""
                            fillMode: Image.PreserveAspectCrop
                            asynchronous: true
                            sourceSize: Qt.size(900, 420)
                        }
                        Rectangle {
                            anchors.fill: parent
                            gradient: Gradient {
                                GradientStop {
                                    position: 0
                                    color: Qt.rgba(0.02, 0.03, 0.09, 0.08)
                                }
                                GradientStop {
                                    position: 1
                                    color: Qt.rgba(0.02, 0.03, 0.09, 0.82)
                                }
                            }
                        }

                        RowLayout {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            anchors.margins: 18
                            spacing: 12

                            Rectangle {
                                Layout.preferredWidth: 54
                                Layout.preferredHeight: 54
                                radius: 27
                                color: Qt.rgba(Theme.accent.r, Theme.accent.g,
                                               Theme.accent.b, 0.18)
                                border.width: 1
                                border.color: Qt.rgba(Theme.accent.r, Theme.accent.g,
                                                      Theme.accent.b, 0.42)
                                clip: true
                                Image {
                                    anchors.fill: parent
                                    source: dashboard.stats.avatar || ""
                                    fillMode: Image.PreserveAspectCrop
                                    visible: source.toString().length > 0
                                        && status !== Image.Error
                                }
                                LunaDashLogo {
                                    anchors.centerIn: parent
                                    width: 32
                                    height: 32
                                    visible: !dashboard.stats.avatar
                                    animated: false
                                }
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                Layout.minimumWidth: 0
                                spacing: 1
                                Text {
                                    Layout.fillWidth: true
                                    text: dashboard.stats.displayName
                                        || dashboard.stats.user || shell.tr("Welcome")
                                    color: Theme.text
                                    font.family: Theme.font
                                    font.pixelSize: 19
                                    font.weight: Font.DemiBold
                                    elide: Text.ElideRight
                                }
                                Text {
                                    Layout.fillWidth: true
                                    text: dashboard.stats.host || shell.tr("Local session")
                                    color: Theme.muted
                                    font.family: Theme.font
                                    font.pixelSize: 10
                                    elide: Text.ElideRight
                                }
                            }

                            ColumnLayout {
                                spacing: 0
                                Text {
                                    Layout.alignment: Qt.AlignRight
                                    text: dashboard.time
                                    color: Theme.moon
                                    font.family: Theme.font
                                    font.pixelSize: 35
                                    font.weight: Font.Light
                                }
                                Text {
                                    Layout.alignment: Qt.AlignRight
                                    text: dashboard.weatherTemperature + "  "
                                        + dashboard.weatherCondition
                                    color: Theme.starlight
                                    font.family: Theme.font
                                    font.pixelSize: 9
                                    elide: Text.ElideRight
                                }
                            }
                        }
                    }

                    Rectangle {
                        Layout.columnSpan: 4
                        Layout.fillWidth: true
                        Layout.preferredHeight: 188
                        Layout.minimumWidth: 0
                        radius: 24
                        color: Theme.surfaceGlass
                        border.width: 1
                        border.color: Theme.hairline

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 15
                            spacing: 8
                            Text {
                                text: shell.tr("Quick actions")
                                color: Theme.text
                                font.family: Theme.font
                                font.pixelSize: 14
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
                                        ["Terminal", "terminal", "terminal"],
                                        ["Plugins", "apps", "plugins"]
                                    ]
                                    ShellButton {
                                        required property var modelData
                                        Layout.fillWidth: true
                                        Layout.fillHeight: true
                                        iconName: modelData[1]
                                        text: shell.tr(modelData[0])
                                        onClicked: {
                                            shell.setAppearance({overview:false})
                                            if (modelData[2] === "settings")
                                                shell.settingsOpen = true
                                            else if (modelData[2] === "plugins")
                                                shell.openSettingsPage("plugins")
                                            else
                                                shell.launch(modelData[2])
                                        }
                                    }
                                }
                            }
                        }
                    }

                    Repeater {
                        model: dashboard.metrics
                        Rectangle {
                            required property var modelData
                            Layout.columnSpan: 3
                            Layout.fillWidth: true
                            Layout.preferredHeight: 116
                            Layout.minimumWidth: 0
                            radius: 20
                            color: Theme.surfaceGlass
                            border.width: 1
                            border.color: Theme.hairline
                            clip: true

                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: 13
                                spacing: 4

                                RowLayout {
                                    Layout.fillWidth: true
                                    LineIcon {
                                        width: 16
                                        height: 16
                                        name: modelData.icon
                                        ink: Theme.accent
                                    }
                                    Text {
                                        Layout.fillWidth: true
                                        text: modelData.title
                                        color: Theme.muted
                                        font.family: Theme.font
                                        font.pixelSize: 9
                                        elide: Text.ElideRight
                                    }
                                    Text {
                                        text: modelData.value
                                        color: Theme.text
                                        font.family: Theme.font
                                        font.pixelSize: 20
                                        font.weight: Font.DemiBold
                                    }
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: modelData.detail
                                    color: Theme.muted
                                    font.family: Theme.font
                                    font.pixelSize: 8
                                    elide: Text.ElideMiddle
                                }

                                Item { Layout.fillHeight: true }

                                Rectangle {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 6
                                    radius: 3
                                    color: Theme.track
                                    clip: true
                                    Rectangle {
                                        width: parent.width * Number(modelData.progress || 0) / 100
                                        height: parent.height
                                        radius: parent.radius
                                        color: Theme.accent
                                        Behavior on width {
                                            NumberAnimation {
                                                duration: Theme.motionFast
                                                easing.type: Easing.OutCubic
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }

                    Rectangle {
                        Layout.columnSpan: 8
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.minimumHeight: 180
                        Layout.minimumWidth: 0
                        radius: 22
                        color: Theme.surfaceGlass
                        border.width: 1
                        border.color: Theme.hairline
                        clip: true

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 14
                            spacing: 8
                            RowLayout {
                                Layout.fillWidth: true
                                Text {
                                    Layout.fillWidth: true
                                    text: shell.tr("Active windows")
                                    color: Theme.text
                                    font.family: Theme.font
                                    font.pixelSize: 13
                                    font.weight: Font.DemiBold
                                }
                                Text {
                                    text: String(dashboard.clients.length)
                                    color: Theme.accent
                                    font.family: Theme.font
                                    font.pixelSize: 10
                                }
                            }

                            ListView {
                                id: windowStrip
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                orientation: ListView.Horizontal
                                spacing: 8
                                clip: true
                                model: dashboard.clients
                                boundsBehavior: Flickable.StopAtBounds

                                delegate: Rectangle {
                                    required property var modelData
                                    width: 126
                                    height: Math.max(96, windowStrip.height - 4)
                                    radius: 16
                                    color: windowMouse.containsMouse
                                        ? Theme.surfaceElevated : Theme.control
                                    border.width: modelData.focused ? 1.5 : 1
                                    border.color: modelData.focused
                                        ? Theme.accent : Theme.hairline

                                    ColumnLayout {
                                        anchors.fill: parent
                                        anchors.margins: 10
                                        spacing: 5
                                        ApplicationIcon {
                                            shell: dashboard.shell
                                            Layout.alignment: Qt.AlignHCenter
                                            width: 32
                                            height: 32
                                            iconName: String(modelData.icon || "")
                                            appId: String(modelData.appId || "")
                                            title: String(modelData.title || "")
                                        }
                                        Text {
                                            Layout.fillWidth: true
                                            text: modelData.title || modelData.appId
                                                || shell.tr("Window")
                                            color: Theme.text
                                            font.family: Theme.font
                                            font.pixelSize: 9
                                            horizontalAlignment: Text.AlignHCenter
                                            elide: Text.ElideRight
                                        }
                                        Text {
                                            Layout.fillWidth: true
                                            text: modelData.appId || ""
                                            color: Theme.muted
                                            font.family: Theme.font
                                            font.pixelSize: 8
                                            horizontalAlignment: Text.AlignHCenter
                                            elide: Text.ElideRight
                                        }
                                    }

                                    MouseArea {
                                        id: windowMouse
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: dashboard.shell.command(
                                            "activate-window", modelData.id)
                                    }
                                }
                            }

                            Text {
                                visible: dashboard.clients.length === 0
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                text: shell.tr("No application windows are open.")
                                color: Theme.muted
                                font.family: Theme.font
                                font.pixelSize: 10
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                        }
                    }

                    Rectangle {
                        Layout.columnSpan: 4
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.minimumHeight: 180
                        Layout.minimumWidth: 0
                        radius: 22
                        color: Theme.surfaceGlass
                        border.width: 1
                        border.color: Theme.hairline

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 15
                            spacing: 8

                            Text {
                                text: shell.tr("Desktop status")
                                color: Theme.text
                                font.family: Theme.font
                                font.pixelSize: 13
                                font.weight: Font.DemiBold
                            }

                            Repeater {
                                model: [
                                    ["network", shell.tr("Network"), dashboard.networkText],
                                    ["sound", shell.tr("Output volume"), dashboard.audioValue],
                                    ["power", shell.tr("Battery"),
                                     Number(dashboard.stats.batteryPercent ?? -1) < 0
                                        ? shell.tr("Desktop")
                                        : Math.round(Number(dashboard.stats.batteryPercent)) + "%"]
                                ]
                                RowLayout {
                                    required property var modelData
                                    Layout.fillWidth: true
                                    LineIcon {
                                        width: 15
                                        height: 15
                                        name: modelData[0]
                                        ink: Theme.accent
                                    }
                                    Text {
                                        Layout.fillWidth: true
                                        text: modelData[1]
                                        color: Theme.muted
                                        font.family: Theme.font
                                        font.pixelSize: 9
                                    }
                                    Text {
                                        Layout.maximumWidth: 150
                                        text: modelData[2]
                                        color: Theme.text
                                        font.family: Theme.font
                                        font.pixelSize: 9
                                        elide: Text.ElideRight
                                    }
                                }
                            }

                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 1
                                color: Theme.hairline
                            }

                            Text {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                text: dashboard.updateInfo.status === "available"
                                    ? shell.tr("A LunaDash update is available.")
                                    : shell.tr("System services are connected.")
                                color: dashboard.updateInfo.status === "available"
                                    ? Theme.warning : Theme.muted
                                font.family: Theme.font
                                font.pixelSize: 9
                                wrapMode: Text.WordWrap
                            }
                        }
                    }
                }
            }

            MediaCard {
                shell: dashboard.shell
            }

            Item {
                GridLayout {
                    anchors.fill: parent
                    columns: 2
                    columnSpacing: 12
                    rowSpacing: 12

                    Repeater {
                        model: dashboard.metrics
                        Rectangle {
                            required property var modelData
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            Layout.minimumWidth: 0
                            Layout.minimumHeight: 230
                            radius: 24
                            color: performanceHover.hovered
                                ? Theme.surfaceElevated : Theme.surfaceGlass
                            border.width: 1
                            border.color: performanceHover.hovered
                                ? Qt.rgba(Theme.accent.r, Theme.accent.g,
                                          Theme.accent.b, 0.42)
                                : Theme.hairline

                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: 20
                                spacing: 8
                                RowLayout {
                                    Layout.fillWidth: true
                                    LineIcon {
                                        width: 20
                                        height: 20
                                        name: modelData.icon
                                        ink: Theme.accent
                                    }
                                    Text {
                                        Layout.fillWidth: true
                                        text: modelData.title
                                        color: Theme.text
                                        font.family: Theme.font
                                        font.pixelSize: 16
                                        font.weight: Font.DemiBold
                                    }
                                    Text {
                                        text: modelData.value
                                        color: Theme.moon
                                        font.family: Theme.font
                                        font.pixelSize: 27
                                        font.weight: Font.Light
                                    }
                                }
                                Text {
                                    Layout.fillWidth: true
                                    text: modelData.detail
                                    color: Theme.muted
                                    font.family: Theme.font
                                    font.pixelSize: 10
                                    elide: Text.ElideMiddle
                                }
                                Item { Layout.fillHeight: true }
                                Rectangle {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 10
                                    radius: 5
                                    color: Theme.track
                                    clip: true
                                    Rectangle {
                                        width: parent.width
                                            * Number(modelData.progress || 0) / 100
                                        height: parent.height
                                        radius: parent.radius
                                        gradient: Gradient {
                                            orientation: Gradient.Horizontal
                                            GradientStop {
                                                position: 0
                                                color: Theme.secondaryAccent
                                            }
                                            GradientStop {
                                                position: 1
                                                color: Theme.accent
                                            }
                                        }
                                        Behavior on width {
                                            NumberAnimation {
                                                duration: Theme.motion
                                                easing.type: Easing.OutCubic
                                            }
                                        }
                                    }
                                }
                                Text {
                                    Layout.fillWidth: true
                                    text: modelData.title === "GPU"
                                          && dashboard.stats.gpuAvailable === false
                                        ? shell.tr("No supported GPU telemetry source was found.")
                                        : shell.tr("Live telemetry refreshes automatically.")
                                    color: Theme.muted
                                    font.family: Theme.font
                                    font.pixelSize: 9
                                    wrapMode: Text.WordWrap
                                }
                            }
                            HoverHandler { id: performanceHover }
                        }
                    }
                }
            }

            Rectangle {
                radius: 26
                color: Theme.surfaceGlass
                border.width: 1
                border.color: Theme.hairline

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 30
                    spacing: 28

                    Rectangle {
                        Layout.preferredWidth: 220
                        Layout.preferredHeight: 220
                        radius: 110
                        color: Qt.rgba(Theme.accent.r, Theme.accent.g,
                                       Theme.accent.b, 0.10)
                        border.width: 1
                        border.color: Qt.rgba(Theme.accent.r, Theme.accent.g,
                                              Theme.accent.b, 0.30)
                        LineIcon {
                            anchors.centerIn: parent
                            width: 90
                            height: 90
                            name: "weather"
                            ink: Theme.moon
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        spacing: 8
                        Text {
                            text: dashboard.weatherTemperature
                            color: Theme.text
                            font.family: Theme.font
                            font.pixelSize: 56
                            font.weight: Font.Light
                        }
                        Text {
                            Layout.fillWidth: true
                            text: dashboard.weatherLocation
                            color: Theme.moon
                            font.family: Theme.font
                            font.pixelSize: 21
                            font.weight: Font.DemiBold
                            elide: Text.ElideRight
                        }
                        Text {
                            Layout.fillWidth: true
                            text: dashboard.weatherCondition
                            color: Theme.muted
                            font.family: Theme.font
                            font.pixelSize: 12
                            wrapMode: Text.WordWrap
                        }
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 1
                            color: Theme.hairline
                        }
                        Text {
                            Layout.fillWidth: true
                            text: weather.temperature === undefined
                                ? shell.tr("Weather is optional. Add a provider later; the dashboard remains fully usable without it.")
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
            dashboard.time = Qt.formatDateTime(
                now, Theme.clock24Hour ? "HH:mm" : "h:mm AP")
            dashboard.date = Qt.formatDateTime(now, "yyyy/MM/dd dddd")
        }
    }
}
