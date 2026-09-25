import QtQuick
import QtQuick.Controls
import QtQuick.Effects
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
    implicitWidth: Math.min(moduleWidth(600), screen
        ? Math.max(1, screen.width - margins.right - moduleMargin) : 600)
    implicitHeight: Math.min(moduleHeight(520), screen
        ? Math.max(1, screen.height - margins.top - Theme.panelBottomInset - moduleMargin) : 520)
    exclusionMode: ExclusionMode.Ignore
    WlrLayershell.layer: WlrLayer.Top
    WlrLayershell.namespace: "lunadash-control-center"
    color: "transparent"
    readonly property int tab: config.showMedia === false && shell.controlCenterTab === 1
        ? 0 : Math.max(0, Math.min(3, shell.controlCenterTab))
    readonly property var config: specification.config || ({})
    readonly property var stats: shell.state.system || ({})
    readonly property var output: (shell.state.audio || {}).output || ({})
    readonly property var input: (shell.state.audio || {}).input || ({})
    readonly property var power: shell.state.power || ({})
    readonly property var weather: shell.state.weather || ({})
    readonly property var appearance: shell.state.appearance || ({})
    readonly property var tabs: [
        {name: "Control center", icon: "apps"},
        {name: "Media", icon: "music"},
        {name: "Sound", icon: "sound"},
        {name: "System", icon: "monitor"}
    ]
    property string clockText: ""
    property string dateText: ""

    function openSettings(page) {
        shell.setAppearance({overview: false});
        shell.openSettingsPage(page);
    }

    component CenterButton: ShellButton {
        id: button
        property string glyph: ""
        implicitWidth: 36
        implicitHeight: 36
        LineIcon {
            anchors.centerIn: parent
            width: 16
            height: 16
            name: button.glyph
            ink: button.active ? Theme.accentInk : Theme.text
        }
    }

    component QuickTile: ShellButton {
        id: tile
        property string caption: ""
        property string tileIcon: ""
        Layout.fillWidth: true
        Layout.minimumWidth: 0
        Layout.preferredHeight: 72
        Layout.fillHeight: true
        clip: true
        Accessible.name: caption
        toolTip: caption
        radius: Theme.radiusMedium
        border.width: 1
        border.color: activeFocus ? Theme.focusRing : Theme.hairline
        ColumnLayout {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.leftMargin: 8
            anchors.rightMargin: 8
            spacing: 4
            LineIcon {
                Layout.alignment: Qt.AlignHCenter
                Layout.preferredWidth: 20
                Layout.preferredHeight: 20
                name: tile.tileIcon
                ink: tile.active ? Theme.accentInk : Theme.text
            }
            Text {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                text: tile.caption
                color: tile.active ? Theme.accentInk : Theme.text
                font.family: Theme.font
                font.pixelSize: 10
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                maximumLineCount: 2
                elide: Text.ElideRight
            }
        }
    }

    MediaController {
        id: compactMedia
        shell: center.shell
        polling: center.opened && center.tab === 0 && (center.config.showMedia ?? true)
    }
    Timer {
        interval: 1000
        running: center.opened
        repeat: true
        triggeredOnStart: true
        onTriggered: {
            center.clockText = Qt.formatDateTime(new Date(), Theme.clock24Hour ? "HH:mm" : "h:mm AP");
            center.dateText = Qt.formatDateTime(new Date(), "ddd, MMM d, yyyy");
        }
    }

    Rectangle {
        anchors.fill: parent
        radius: Theme.radiusLarge
        color: Theme.surfaceStrong
        border.width: 1
        border.color: Theme.hairline
        clip: true
        RowLayout {
            anchors.fill: parent
            anchors.margins: 12
            spacing: 12
            Rectangle {
                Layout.preferredWidth: 44
                Layout.minimumWidth: 44
                Layout.fillHeight: true
                radius: Theme.radiusMedium
                color: Theme.surface
                ScrollView {
                    id: navigation
                    anchors.fill: parent
                    anchors.margins: 4
                    clip: true
                    contentWidth: availableWidth
                    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                    ColumnLayout {
                        width: navigation.availableWidth
                        spacing: 4
                        Repeater {
                            model: center.tabs
                            CenterButton {
                                required property var modelData
                                required property int index
                                Layout.fillWidth: true
                                Layout.preferredHeight: 36
                                glyph: modelData.icon
                                quiet: true
                                active: center.tab === index
                                visible: index !== 1 || (center.config.showMedia ?? true)
                                toolTip: shell.tr(modelData.name)
                                Accessible.name: toolTip
                                onClicked: shell.controlCenterTab = index
                            }
                        }
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.margins: 5
                            Layout.preferredHeight: 1
                            color: Theme.hairline
                        }
                        Repeater {
                            model: [
                                {name: "Display", icon: "display", page: "display"},
                                {name: "Network", icon: "network", page: "network"},
                                {name: "Bluetooth", icon: "bluetooth", page: "bluetooth"},
                                {name: "Power and battery", icon: "power", page: "power"},
                                {name: "Appearance", icon: "appearance", page: "appearance"},
                                {name: "Users, date and time", icon: "system", page: "system"}
                            ]
                            CenterButton {
                                required property var modelData
                                Layout.fillWidth: true
                                Layout.preferredHeight: 36
                                glyph: modelData.icon
                                quiet: true
                                toolTip: shell.tr(modelData.name)
                                Accessible.name: toolTip
                                onClicked: center.openSettings(modelData.page)
                            }
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
                    Layout.minimumWidth: 0
                    spacing: 6
                    Text {
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        text: shell.tr(center.tabs[center.tab].name)
                        color: Theme.accent
                        font.family: Theme.font
                        font.pixelSize: 15
                        font.weight: Font.DemiBold
                        elide: Text.ElideRight
                    }
                    CenterButton {
                        glyph: "settings"
                        toolTip: shell.tr("Settings")
                        Accessible.name: toolTip
                        onClicked: center.openSettings("general")
                    }
                    CenterButton {
                        glyph: "session"
                        toolTip: shell.tr("Session controls")
                        Accessible.name: toolTip
                        onClicked: {
                            shell.setAppearance({overview: false});
                            shell.logoutOpen = true;
                        }
                    }
                    CenterButton {
                        glyph: "close"
                        toolTip: shell.tr("Close")
                        Accessible.name: toolTip
                        onClicked: shell.setAppearance({overview: false})
                    }
                }
                ScrollView {
                    id: mediaScroll
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.minimumWidth: 0
                    Layout.minimumHeight: 0
                    visible: center.tab === 1
                    clip: true
                    contentWidth: Math.max(380, availableWidth)
                    contentHeight: Math.max(400, availableHeight)
                    MediaCard {
                        width: mediaScroll.contentWidth
                        height: mediaScroll.contentHeight
                        shell: center.shell
                    }
                }
                ScrollView {
                    id: contentScroll
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.minimumWidth: 0
                    Layout.minimumHeight: 0
                    visible: center.tab !== 1
                    clip: true
                    contentWidth: availableWidth
                    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                    ColumnLayout {
                        width: contentScroll.availableWidth
                        spacing: 12
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.minimumWidth: 0
                            Layout.preferredHeight: 176
                            visible: center.tab === 0
                            radius: Theme.radiusMedium
                            color: Theme.surface
                            border.width: 1
                            border.color: Theme.hairline
                            clip: true
                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: 16
                                spacing: 14
                                Rectangle {
                                    Layout.preferredWidth: contentScroll.availableWidth < 420 ? 64 : 100
                                    Layout.preferredHeight: width
                                    radius: width / 2
                                    color: Theme.surfaceElevated
                                    border.width: 2
                                    border.color: Theme.accent
                                    clip: true
                                    Image {
                                        id: accountImage
                                        anchors.fill: parent
                                        anchors.margins: 4
                                        source: center.stats.avatar || ""
                                        fillMode: Image.PreserveAspectCrop
                                        asynchronous: true
                                        visible: false
                                    }
                                    Rectangle {
                                        id: accountMask
                                        anchors.fill: accountImage
                                        radius: width / 2
                                        color: "white"
                                        visible: false
                                        layer.enabled: true
                                    }
                                    MultiEffect {
                                        anchors.fill: accountImage
                                        source: accountImage
                                        visible: accountImage.status === Image.Ready
                                        autoPaddingEnabled: false
                                        maskEnabled: true
                                        maskSource: accountMask
                                    }
                                    LunaDashLogo {
                                        anchors.centerIn: parent
                                        width: parent.width * 0.55
                                        height: width
                                        visible: accountImage.status !== Image.Ready
                                        animated: false
                                    }
                                }
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    Layout.minimumWidth: 0
                                    spacing: 6
                                    Text {
                                        Layout.fillWidth: true
                                        Layout.minimumWidth: 0
                                        text: center.stats.displayName || center.stats.user || shell.tr("User")
                                        textFormat: Text.PlainText
                                        color: Theme.text
                                        font.family: Theme.font
                                        font.pixelSize: 18
                                        font.weight: Font.DemiBold
                                        elide: Text.ElideRight
                                    }
                                    Text {
                                        Layout.fillWidth: true
                                        Layout.minimumWidth: 0
                                        text: [center.stats.user, center.stats.host].filter(Boolean).join("@")
                                        textFormat: Text.PlainText
                                        color: Theme.muted
                                        font.family: Theme.font
                                        font.pixelSize: 12
                                        elide: Text.ElideRight
                                    }
                                    Text {
                                        Layout.fillWidth: true
                                        Layout.minimumWidth: 0
                                        text: center.stats.os || shell.tr("Local session")
                                        textFormat: Text.PlainText
                                        color: Theme.muted
                                        font.family: Theme.font
                                        font.pixelSize: 12
                                        elide: Text.ElideRight
                                    }
                                    Text {
                                        Layout.fillWidth: true
                                        Layout.minimumWidth: 0
                                        text: "LunaDash " + ((shell.state.update || {}).currentVersion || "1.0.1a")
                                        color: Theme.muted
                                        font.family: Theme.font
                                        font.pixelSize: 12
                                        elide: Text.ElideRight
                                    }
                                }
                            }
                        }
                        GridLayout {
                            Layout.fillWidth: true
                            Layout.minimumWidth: 0
                            columns: contentScroll.availableWidth < 420 ? 1 : 2
                            columnSpacing: 12
                            rowSpacing: 12
                            visible: center.tab === 0
                            ColumnLayout {
                                Layout.fillWidth: true
                                Layout.minimumWidth: 0
                                Layout.preferredWidth: 300
                                Layout.preferredHeight: 234
                                spacing: 10
                                Rectangle {
                                    Layout.fillWidth: true
                                    Layout.minimumWidth: 0
                                    Layout.preferredHeight: 138
                                    visible: center.config.showMedia ?? true
                                    radius: Theme.radiusMedium
                                    color: Theme.surface
                                    border.width: 1
                                    border.color: Theme.hairline
                                    clip: true
                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.margins: 12
                                        spacing: 8
                                        MediaDisc {
                                            Layout.preferredWidth: 66
                                            Layout.preferredHeight: 66
                                            artwork: compactMedia.media.artUrl || ""
                                            playing: Boolean(compactMedia.media.playing)
                                        }
                                        ColumnLayout {
                                            Layout.fillWidth: true
                                            Layout.minimumWidth: 0
                                            spacing: 4
                                            Text {
                                                Layout.fillWidth: true
                                                Layout.minimumWidth: 0
                                                text: compactMedia.media.available
                                                    ? compactMedia.media.title || shell.tr("Unknown track") : shell.tr("Nothing is playing")
                                                textFormat: Text.PlainText
                                                color: Theme.text
                                                font.family: Theme.font
                                                font.pixelSize: 12
                                                wrapMode: Text.Wrap
                                                maximumLineCount: 2
                                                elide: Text.ElideRight
                                            }
                                            Text {
                                                Layout.fillWidth: true
                                                Layout.minimumWidth: 0
                                                text: compactMedia.media.artist || compactMedia.media.identity || shell.tr("Media")
                                                textFormat: Text.PlainText
                                                color: Theme.muted
                                                font.family: Theme.font
                                                font.pixelSize: 11
                                                elide: Text.ElideRight
                                            }
                                            RowLayout {
                                                Layout.fillWidth: true
                                                spacing: 4
                                                CenterButton {
                                                    glyph: compactMedia.media.playing ? "pause" : "play"
                                                    quiet: true
                                                    visible: Boolean(compactMedia.media.available)
                                                    enabled: Boolean(compactMedia.media.playing
                                                        ? compactMedia.media.canPause : compactMedia.media.canPlay)
                                                    toolTip: shell.tr(compactMedia.media.playing ? "Pause" : "Play")
                                                    Accessible.name: toolTip
                                                    onClicked: compactMedia.run("play-pause", "")
                                                }
                                                CenterButton {
                                                    glyph: "chevronRight"
                                                    quiet: true
                                                    toolTip: shell.tr("Media")
                                                    Accessible.name: toolTip
                                                    onClicked: shell.controlCenterTab = 1
                                                }
                                                Item { Layout.fillWidth: true }
                                            }
                                        }
                                    }
                                }
                                Rectangle {
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    Layout.minimumWidth: 0
                                    Layout.minimumHeight: 86
                                    radius: Theme.radiusMedium
                                    color: Theme.surface
                                    border.width: 1
                                    border.color: Theme.hairline
                                    clip: true
                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.margins: 12
                                        spacing: 10
                                        Text {
                                            Layout.preferredWidth: implicitWidth
                                            text: center.clockText
                                            color: Theme.accent
                                            font.family: Theme.font
                                            font.pixelSize: Theme.clock24Hour ? 28 : 22
                                            font.weight: Font.DemiBold
                                        }
                                        ColumnLayout {
                                            Layout.fillWidth: true
                                            Layout.minimumWidth: 0
                                            spacing: 4
                                            Text {
                                                Layout.fillWidth: true
                                                Layout.minimumWidth: 0
                                                text: center.dateText
                                                color: Theme.text
                                                font.family: Theme.font
                                                font.pixelSize: 10
                                                wrapMode: Text.Wrap
                                                maximumLineCount: 2
                                                elide: Text.ElideRight
                                            }
                                            Text {
                                                Layout.fillWidth: true
                                                Layout.minimumWidth: 0
                                                text: center.weather.temperature !== undefined
                                                    ? String(center.weather.temperature) + "° · " + (center.weather.condition || center.weather.summary || "")
                                                    : shell.tr("Weather data unavailable")
                                                textFormat: Text.PlainText
                                                color: Theme.muted
                                                font.family: Theme.font
                                                font.pixelSize: 10
                                                wrapMode: Text.Wrap
                                                maximumLineCount: 2
                                                elide: Text.ElideRight
                                            }
                                        }
                                    }
                                }
                            }
                            GridLayout {
                                Layout.fillWidth: true
                                Layout.minimumWidth: 0
                                Layout.preferredWidth: 180
                                Layout.preferredHeight: 234
                                columns: 2
                                columnSpacing: 8
                                rowSpacing: 8
                                visible: center.config.quickControls ?? true
                                QuickTile {
                                    tileIcon: "network"
                                    caption: shell.tr("Network")
                                    active: Boolean((shell.state.network || {}).connected)
                                    onClicked: {
                                        shell.setAppearance({overview: false});
                                        shell.wifiPopupOpen = true;
                                    }
                                }
                                QuickTile {
                                    tileIcon: "bluetooth"
                                    caption: shell.tr("Bluetooth")
                                    onClicked: center.openSettings("bluetooth")
                                }
                                QuickTile {
                                    tileIcon: "moon"
                                    caption: shell.tr("Eye care")
                                    active: Boolean(center.appearance.eyeCare)
                                    onClicked: shell.command("eye-care", "toggle")
                                }
                                QuickTile {
                                    tileIcon: "appearance"
                                    caption: shell.tr(Theme.dark ? "Dark" : "Light")
                                    active: Theme.dark
                                    onClicked: shell.setAppearance({themeMode: Theme.dark ? "light" : "dark"})
                                }
                                QuickTile {
                                    tileIcon: "privacy"
                                    caption: shell.tr("Quiet notifications")
                                    active: center.appearance.notificationsEnabled === false
                                    onClicked: shell.setAppearance({notificationsEnabled: active})
                                }
                                QuickTile {
                                    tileIcon: "power"
                                    caption: shell.tr(center.power.current || "Power profile")
                                    active: center.power.current === "performance"
                                    onClicked: center.openSettings("power")
                                }
                            }
                        }
                        Repeater {
                            model: center.tab === 2 ? ["output", "input"] : []
                            Rectangle {
                                required property string modelData
                                readonly property var device: modelData === "output" ? center.output : center.input
                                Layout.fillWidth: true
                                Layout.minimumWidth: 0
                                Layout.preferredHeight: 106
                                radius: Theme.radiusMedium
                                color: Theme.surface
                                border.width: 1
                                border.color: Theme.hairline
                                clip: true
                                ColumnLayout {
                                    anchors.fill: parent
                                    anchors.margins: 12
                                    RowLayout {
                                        Layout.fillWidth: true
                                        Text {
                                            Layout.fillWidth: true
                                            Layout.minimumWidth: 0
                                            text: shell.tr(modelData === "output" ? "Output volume" : "Microphone")
                                            color: Theme.text
                                            font.family: Theme.font
                                            elide: Text.ElideRight
                                        }
                                        ShellButton {
                                            text: device.muted ? shell.tr("Unmute") : shell.tr("Mute")
                                            active: device.muted ?? false
                                            enabled: device.available !== false
                                            onClicked: shell.command("audio", JSON.stringify({device: modelData, mute: !device.muted}))
                                        }
                                    }
                                    SoftSlider {
                                        Layout.fillWidth: true
                                        from: 0
                                        to: 100
                                        value: device.volume || 0
                                        enabled: device.available !== false
                                        Accessible.name: shell.tr(modelData === "output" ? "Output volume" : "Microphone")
                                        onPressedChanged: if (!pressed)
                                            shell.command("audio", JSON.stringify({device: modelData, volume: Math.round(value)}))
                                    }
                                }
                            }
                        }
                        Repeater {
                            model: center.tab === 3 ? [
                                {name: "CPU", value: Math.round(center.stats.cpuPercent || 0) + "%", detail: center.stats.cpuModel || ""},
                                {name: "Memory", value: Math.round(center.stats.memoryPercent || 0) + "%",
                                    detail: Number(center.stats.memoryUsed || 0).toFixed(1) + " / " + Number(center.stats.memoryTotal || 0).toFixed(1) + " GiB"},
                                {name: "Storage", value: Math.round(center.stats.diskPercent || 0) + "%", detail: center.stats.diskDevice || ""}
                            ] : []
                            Rectangle {
                                required property var modelData
                                Layout.fillWidth: true
                                Layout.minimumWidth: 0
                                Layout.preferredHeight: 110
                                radius: Theme.radiusMedium
                                color: Theme.surface
                                border.width: 1
                                border.color: Theme.hairline
                                clip: true
                                ColumnLayout {
                                    anchors.fill: parent
                                    anchors.margins: 16
                                    Text {
                                        Layout.fillWidth: true
                                        Layout.minimumWidth: 0
                                        text: shell.tr(modelData.name) + "  " + modelData.value
                                        color: Theme.accent
                                        font.family: Theme.font
                                        font.pixelSize: 24
                                        elide: Text.ElideRight
                                    }
                                    Text {
                                        Layout.fillWidth: true
                                        Layout.minimumWidth: 0
                                        text: modelData.detail
                                        textFormat: Text.PlainText
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
