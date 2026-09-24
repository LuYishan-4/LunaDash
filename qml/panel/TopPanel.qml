import "../modules"
import "../plugins"
import QtQuick
import "../columns/WorkspaceTasks.js" as WorkspaceTasks
import QtQuick.Controls
import QtQuick.Window
import Quickshell
import Quickshell.Services.SystemTray
import Quickshell.Wayland
import Quickshell.Widgets
import "../columns"
import "../overview"
import "../components"
import "../style"

ModuleSurface {
    id: panel
    moduleId: "panel"

    readonly property string edge: ["top", "bottom", "left", "right"].includes(moduleStyle.edge)
        ? moduleStyle.edge : "top"
    readonly property bool vertical: edge === "left" || edge === "right"
    readonly property int thickness: Math.max(
        42, moduleStyle.height || (shell.state.appearance || {}).panelHeight || 40)
    readonly property int requestedLength: moduleStyle.width || 0

    anchors {
        top: edge === "top" || (vertical && !requestedLength)
        bottom: edge === "bottom" || (vertical && !requestedLength)
        left: edge === "left" || (!vertical && !requestedLength)
        right: edge === "right" || (!vertical && !requestedLength)
    }
    margins {
        top: edge === "top" ? moduleMargin + 4 : moduleMargin
        bottom: edge === "bottom" ? moduleMargin + 4 : moduleMargin
        left: edge === "left" ? moduleMargin + 4 : moduleMargin
        right: edge === "right" ? moduleMargin + 4 : moduleMargin
    }

    implicitWidth: vertical
        ? thickness
        : Math.max(1, Math.min(requestedLength || (screen ? screen.width : 1440),
                              screen ? screen.width - 2 * moduleMargin : 3840))
    implicitHeight: vertical
        ? Math.max(1, Math.min(requestedLength || (screen ? screen.height : 900),
                              screen ? screen.height - 2 * moduleMargin : 2160))
        : thickness
    exclusiveZone: (vertical ? implicitWidth : implicitHeight) + moduleMargin + 6
    color: "transparent"
    WlrLayershell.namespace: "lunadash-panel"

    property var stats: shell.state.system || ({})
    readonly property var panelConfig: specification.config || ({})
    readonly property bool backgroundVisible: panelConfig.backgroundVisible ?? false
    readonly property bool contrastShells: panelConfig.contrastShells ?? true
    readonly property bool workspacePills: panelConfig.workspacePills ?? false
    readonly property int workspaceInactiveWidth: panelConfig.workspaceInactiveWidth ?? 16
    readonly property int workspaceActiveWidth: panelConfig.workspaceActiveWidth ?? 34
    readonly property int workspacePillHeight: panelConfig.workspacePillHeight ?? 8
    readonly property real shellOpacity: (panelConfig.shellOpacity ?? 26) / 100.0
    readonly property int capsuleHeight: Math.max(30, implicitHeight - 8)
    readonly property int capsuleGap: 8
    readonly property var networkState: shell.state.network || ({})

    readonly property var groups: WorkspaceTasks.groupByWorkspace(
        (shell.interaction || {}).clients || shell.state.clients || [],
        (shell.interaction || {}).workspace ?? shell.state.workspace)

    readonly property var launcherModule: (((shell.state.shellModules || {}).modules || {}).launcher || ({}))
    readonly property var launcherConfig: launcherModule.config || ({})
    readonly property var launcherStyle: launcherModule.style || ({})
    readonly property color launcherAccent: !launcherStyle.accent || launcherStyle.accent === "inherit"
        ? Theme.accent
        : launcherStyle.accent
    readonly property var usbStorage: (shell.removableDevices || []).filter(device => device.storage)

    readonly property int focusedGroupIndex: {
        for (let index = 0; index < groups.length; ++index)
            if (groups[index].active)
                return index
        return -1
    }

    onFocusedGroupIndexChanged: {
        if (focusedGroupIndex < 0)
            return
        if (panel.vertical)
            verticalTasks.positionViewAtIndex(focusedGroupIndex, ListView.Contain)
        else
            columnTasks.positionViewAtIndex(focusedGroupIndex, ListView.Contain)
    }
    onGroupsChanged: Qt.callLater(() => {
        if (focusedGroupIndex < 0)
            return
        if (panel.vertical)
            verticalTasks.positionViewAtIndex(focusedGroupIndex, ListView.Contain)
        else
            columnTasks.positionViewAtIndex(focusedGroupIndex, ListView.Contain)
    })

    function capsuleColor(tintAmount, alpha) {
        const t = Math.max(0, Math.min(1, tintAmount))
        return Qt.rgba(
            moduleBackground.r * (1 - t) + Theme.secondaryAccent.r * t,
            moduleBackground.g * (1 - t) + Theme.secondaryAccent.g * t,
            moduleBackground.b * (1 - t) + Theme.secondaryAccent.b * t,
            Theme.eyeCare ? 1 : Math.min(alpha, 0.84)
        )
    }
    function capsuleBorder(alpha) {
        return Qt.rgba(moduleAccent.r, moduleAccent.g, moduleAccent.b, alpha)
    }
    function trayImage(item) {
        const supplied = String(item.icon || "")
        if (/^(image:|file:|qrc:|data:)/.test(supplied) && !supplied.includes("qs-blackhole"))
            return supplied
        if (supplied.startsWith("/") && /\.(png|jpe?g|webp|svg|xpm)$/i.test(supplied))
            return "file://" + supplied
        return ""
    }
    function trayGlyph(item) {
        const identity = (String(item.id || "") + " " + String(item.title || "")).toLowerCase()
        if (identity.includes("fcitx") || identity.includes("input") || identity.includes("keyboard")) return "input"
        if (identity.includes("discord")) return "discord"
        if (identity.includes("docker")) return "apps"
        if (identity.includes("network") || identity.includes("wifi")) return "network"
        if (identity.includes("bluetooth")) return "bluetooth"
        if (identity.includes("sound") || identity.includes("audio") || identity.includes("volume")) return "sound"
        if (identity.includes("battery") || identity.includes("power")) return "power"
        return "apps"
    }
    function networkLabel() {
        if (networkState.ethernetConnected)
            return shell.tr("Ethernet") + (networkState.primaryConnection ? ": " + networkState.primaryConnection : "")
        if (networkState.wifiConnected)
            return shell.tr("Wi-Fi") + (networkState.primaryConnection ? ": " + networkState.primaryConnection : "")
        return shell.tr("Disconnected")
    }

    Rectangle {
        anchors.fill: parent
        visible: panel.backgroundVisible
        radius: height / 2
        color: panel.capsuleColor(0.20, 0.58)
        border.width: 1
        border.color: panel.capsuleBorder(0.22)
    }

    Item {
        id: workspaceShell
        visible: !panel.vertical
        anchors { left: centerShell.right; leftMargin: panel.capsuleGap; verticalCenter: parent.verticalCenter }
        height: panel.capsuleHeight
        width: Math.min(workspaceControls.implicitWidth + 18, Math.max(0, clockShell.x - centerShell.width - panel.capsuleGap * 3))
        clip: true
        Rectangle {
            anchors.fill: parent
            visible: panel.contrastShells
            radius: height / 2
            color: panel.capsuleColor(0.72, 0.94)
            border.width: 1
            border.color: panel.capsuleBorder(0.40)
        }
        Row {
            id: workspaceControls
            anchors.centerIn: parent
            spacing: 6
            Repeater {
                model: Math.max((shell.state.appearance || {}).workspaceCount || 10, Number((shell.interaction || {}).workspace || 0) + 1)
                Item {
                    id: workspacePill
                    required property int index
                    readonly property bool active: Number((shell.interaction || {}).workspace ?? shell.state.workspace) === index
                    width: panel.workspacePills ? (active ? panel.workspaceActiveWidth : panel.workspaceInactiveWidth) : 22
                    height: workspaceShell.height
                    Accessible.role: Accessible.Button
                    Accessible.name: shell.tr("Workspace") + " " + (index + 1)
                    Rectangle {
                        anchors.centerIn: parent
                        width: parent.width
                        height: panel.workspacePills ? panel.workspacePillHeight : 18
                        radius: height / 2
                        color: workspacePill.active
                            ? moduleAccent
                            : Qt.rgba(moduleForeground.r, moduleForeground.g, moduleForeground.b,
                                      workspaceMouse.containsMouse ? 0.30 : 0.14)
                        border.width: workspacePill.active ? 0 : 1
                        border.color: panel.capsuleBorder(0.30)
                        scale: workspaceMouse.pressed ? 0.90 : workspaceMouse.containsMouse ? 1.06 : 1.0
                        Text {
                            visible: !panel.workspacePills
                            anchors.centerIn: parent
                            text: String(workspacePill.index + 1)
                            color: workspacePill.active ? Theme.accentInk : Theme.text
                            font.family: Theme.font
                            font.pixelSize: 10
                            font.weight: Font.DemiBold
                        }
                        Behavior on width { NumberAnimation { duration: Math.max(150, Theme.motion); easing.type: Easing.OutCubic } }
                        Behavior on color { ColorAnimation { duration: Theme.motion } }
                        Behavior on scale { NumberAnimation { duration: Math.max(100, Theme.motion); easing.type: Easing.OutCubic } }
                    }
                    MouseArea {
                        id: workspaceMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: shell.command("workspace", workspacePill.index)
                    }
                }
            }
            Rectangle { width: 1; height: 16; anchors.verticalCenter: parent.verticalCenter; color: panel.capsuleBorder(0.24) }
            PanelSegment {
                moduleHost: panel
                text: "◈"
                implicitWidth: 26
                fill: "transparent"
                border.width: 0
                ink: Theme.danger
                Accessible.name: shell.tr("Control center")
                onClicked: { shell.controlCenterTab = 0; shell.setAppearance({overview: !shell.overviewOpen}) }
            }
        }
    }

    Item {
        id: taskShell
        visible: !panel.vertical && columnTasks.count > 0
        anchors { left: workspaceShell.right; leftMargin: panel.capsuleGap; verticalCenter: parent.verticalCenter }
        height: panel.capsuleHeight
        width: Math.min(columnTasks.contentWidth + 8, Math.max(0, clockShell.x - x - panel.capsuleGap))
        ExtensionSlot {
            anchors.fill: parent
            shell: panel.shell
            target: "taskbar-windows"
            context: ({groups: panel.groups})
        ListView {
            id: columnTasks
            anchors.fill: parent
            anchors.margins: 4
            orientation: ListView.Horizontal
            spacing: 8
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            model: panel.groups.length
            visible: count > 0
            Behavior on contentX {
                enabled: !columnTasks.flicking && !columnTasks.moving
                NumberAnimation { duration: Math.max(120, Theme.motion); easing.type: Easing.OutCubic }
            }
            delegate: ColumnCell {
                required property int index
                shell: panel.shell
                group: panel.groups[index] || ({})
                height: columnTasks.height
                width: implicitWidth
            }
        }
    }

    }

    Item {
        id: centerShell
        visible: !panel.vertical
        anchors { left: parent.left; leftMargin: 8; verticalCenter: parent.verticalCenter }
        width: 80
        height: panel.capsuleHeight
        Rectangle { anchors.fill: parent; radius: height / 2; color: Theme.surfaceGlass; border.width: 1; border.color: Theme.hairline }
        Row {
            anchors.centerIn: parent
            spacing: 2
            PanelSegment { moduleHost: panel; text: "⌕"; implicitWidth: 32; fill: "transparent"; border.width: 0; Accessible.name: shell.tr("Applications"); onClicked: shell.openLauncherFromMouse() }
            PanelSegment { moduleHost: panel; text: "◎"; implicitWidth: 32; fill: "transparent"; border.width: 0; Accessible.name: shell.tr("Orbit launcher"); onClicked: shell.orbitOpen = !shell.orbitOpen }
        }
    }

    Item {
        id: statusShell
        visible: !panel.vertical
        anchors { right: parent.right; rightMargin: 8; verticalCenter: parent.verticalCenter }
        height: panel.capsuleHeight
        width: statusControls.implicitWidth + 14
        Rectangle {
            anchors.fill: parent
            visible: panel.contrastShells
            radius: height / 2
            color: panel.capsuleColor(0.76, 0.94)
            border.width: 1
            border.color: panel.capsuleBorder(0.34)
        }
        Row {
            id: statusControls
            anchors.centerIn: parent
            height: parent.height
            spacing: 3
            Repeater {
                model: SystemTray.items
                delegate: Item {
                    id: trayDelegate
                    required property var modelData
                    readonly property var item: modelData
                    visible: item.status !== Status.Passive
                    width: visible ? Math.min(28, panel.height - 8) : 0
                    height: Math.min(28, panel.height - 8)
                    Rectangle {
                        anchors.fill: parent
                        radius: height / 2
                        color: trayMouse.containsMouse ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.24) : "transparent"
                    }
                    readonly property string iconSource: panel.trayImage(item)
                    readonly property string glyph: panel.trayGlyph(item)
                    Image {
                        id: trayIcon
                        anchors.centerIn: parent
                        width: Math.max(10, Math.min(18, parent.width - 4))
                        height: width
                        source: trayDelegate.iconSource
                        sourceSize.width: Math.max(1, Math.round(width * Screen.devicePixelRatio))
                        sourceSize.height: Math.max(1, Math.round(height * Screen.devicePixelRatio))
                        fillMode: Image.PreserveAspectFit
                        smooth: true
                        mipmap: true
                        visible: trayDelegate.iconSource.length > 0 && status === Image.Ready
                    }
                    LineIcon { anchors.centerIn: parent; width: 17; height: 17; visible: !trayIcon.visible; name: trayDelegate.glyph; ink: Theme.text }
                    MouseArea {
                        id: trayMouse
                        anchors.fill: parent
                        acceptedButtons: Qt.LeftButton | Qt.RightButton | Qt.MiddleButton
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: mouse => {
                            if (mouse.button === Qt.LeftButton) trayDelegate.item.activate()
                            else trayDelegate.item.secondaryActivate()
                        }
                        onWheel: wheel => trayDelegate.item.scroll(wheel.angleDelta.y || wheel.angleDelta.x, wheel.angleDelta.x !== 0)
                    }
                    ToolTip.visible: trayMouse.containsMouse
                    ToolTip.delay: 450
                    ToolTip.text: item.tooltipTitle || item.title || item.id
                }
            }

            Item {
                id: clipboardIndicator
                width: 30
                height: 28
                scale: clipboardMouse.pressed ? 0.92
                    : clipboardMouse.containsMouse || shell.clipboardPopupOpen ? 1.07 : 1
                Accessible.role: Accessible.Button
                Accessible.name: shell.tr("Clipboard")
                Behavior on scale { NumberAnimation { duration: Theme.motionFast; easing.type: Easing.OutCubic } }
                Rectangle {
                    anchors.fill: parent
                    radius: height / 2
                    color: clipboardMouse.containsMouse || shell.clipboardPopupOpen
                        ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.22)
                        : "transparent"
                    Behavior on color { ColorAnimation { duration: Theme.motionFast } }
                }
                LineIcon {
                    anchors.centerIn: parent
                    width: 17
                    height: 17
                    name: "copy"
                    ink: shell.clipboardPopupOpen ? Theme.accent : Theme.text
                }
                MouseArea {
                    id: clipboardMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: shell.clipboardPopupOpen = !shell.clipboardPopupOpen
                }
                ToolTip.visible: clipboardMouse.containsMouse
                ToolTip.delay: 450
                ToolTip.text: shell.tr("Clipboard")
            }

            Item {
                id: usbIndicator
                visible: panel.usbStorage.length > 0
                width: visible ? 30 : 0
                height: 28
                Accessible.role: Accessible.Button
                Accessible.name: shell.tr("USB devices")
                Rectangle {
                    anchors.fill: parent
                    radius: height / 2
                    color: usbMouse.containsMouse || shell.usbPopupOpen
                        ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.24)
                        : Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.08)
                }
                LineIcon { anchors.centerIn: parent; width: 17; height: 17; name: "usb"; ink: Theme.accent }
                Rectangle {
                    visible: panel.usbStorage.length > 1
                    anchors.right: parent.right
                    anchors.top: parent.top
                    width: 12; height: 12; radius: 6; color: Theme.accent
                    Text { anchors.centerIn: parent; text: String(Math.min(9, panel.usbStorage.length)); color: Theme.background; font.family: Theme.font; font.pixelSize: 8; font.weight: Font.Bold }
                }
                MouseArea { id: usbMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: shell.usbPopupOpen = !shell.usbPopupOpen }
                ToolTip.visible: usbMouse.containsMouse
                ToolTip.delay: 450
                ToolTip.text: shell.tr("USB devices")
            }

            Item {
                visible: panel.width > 710
                width: visible ? 30 : 0
                height: 28
                scale: volumeMouse.pressed ? 0.92 : volumeMouse.containsMouse || shell.volumePopupOpen ? 1.07 : 1
                Accessible.role: Accessible.Button
                Accessible.name: shell.tr("Volume")
                Behavior on scale { NumberAnimation { duration: Theme.motionFast; easing.type: Easing.OutCubic } }
                Rectangle {
                    anchors.fill: parent
                    radius: height / 2
                    color: volumeMouse.containsMouse || shell.volumePopupOpen
                        ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.22)
                        : "transparent"
                    Behavior on color { ColorAnimation { duration: Theme.motionFast } }
                }
                LineIcon {
                    anchors.centerIn: parent
                    width: 17
                    height: 17
                    name: "sound"
                    ink: ((shell.state.audio || {}).output || {}).muted ? Theme.muted : Theme.text
                }
                MouseArea {
                    id: volumeMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: shell.volumePopupOpen = !shell.volumePopupOpen
                    onWheel: wheel => {
                        const current = Number((((shell.state.audio || {}).output || {}).volume) || 0)
                        const next = Math.max(0, Math.min(100, current + (wheel.angleDelta.y > 0 ? 5 : -5)))
                        shell.command("audio", JSON.stringify({device:"output", volume:next}))
                    }
                }
                ToolTip.visible: volumeMouse.containsMouse
                ToolTip.delay: 450
                ToolTip.text: shell.tr("Volume") + " " + Math.round((((shell.state.audio || {}).output || {}).volume) || 0) + "%"
            }

            Item {
                visible: true
                width: 30
                height: 28
                scale: networkMouse.pressed ? 0.92 : networkMouse.containsMouse || shell.wifiPopupOpen ? 1.07 : 1
                Accessible.role: Accessible.Button
                Accessible.name: panel.networkLabel()
                Behavior on scale { NumberAnimation { duration: Theme.motionFast; easing.type: Easing.OutCubic } }
                Rectangle {
                    anchors.fill: parent
                    radius: height / 2
                    color: networkMouse.containsMouse || shell.wifiPopupOpen
                        ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.22)
                        : panel.networkState.connected
                            ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.07)
                            : Qt.rgba(Theme.danger.r, Theme.danger.g, Theme.danger.b, 0.06)
                    Behavior on color { ColorAnimation { duration: Theme.motionFast } }
                }
                LineIcon {
                    anchors.centerIn: parent
                    width: 17
                    height: 17
                    name: panel.networkState.ethernetConnected
                        ? "ethernet"
                        : panel.networkState.connected
                            ? "network"
                            : "network-off"
                    ink: panel.networkState.connected ? Theme.accent : Theme.muted
                }
                MouseArea {
                    id: networkMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: shell.wifiPopupOpen = !shell.wifiPopupOpen
                }
                ToolTip.visible: networkMouse.containsMouse
                ToolTip.delay: 450
                ToolTip.text: panel.networkLabel()
            }

            PanelSegment {
                moduleHost: panel
                visible: panel.width > 900 && (panel.stats.batteryPercent ?? -1) >= 0
                text: panel.stats.batteryPercent + "%"
                implicitWidth: visible ? 48 : 0
                fill: "transparent"
                border.width: 0
                Accessible.name: shell.tr("Battery")
                onClicked: shell.openSettingsPage("power")
            }
        }
    }

    Item {
        id: clockShell
        visible: !panel.vertical
        anchors.centerIn: parent
        height: panel.capsuleHeight
        width: panel.width > 1120 ? 156 : 78
        scale: clockMouse.pressed ? 0.97 : clockMouse.containsMouse || shell.calendarOpen ? 1.012 : 1
        Behavior on width { NumberAnimation { duration: Theme.motion; easing.type: Easing.OutCubic } }
        Behavior on scale { NumberAnimation { duration: Theme.motionFast; easing.type: Easing.OutCubic } }
        property string time: ""
        property string date: ""
        Accessible.role: Accessible.Button
        Accessible.name: shell.tr("Calendar")
        Rectangle {
            anchors.fill: parent
            visible: panel.contrastShells
            radius: height / 2
            color: clockMouse.containsMouse || shell.calendarOpen
                ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.16)
                : panel.capsuleColor(0.72, 0.95)
            border.width: 1
            border.color: clockMouse.containsMouse || shell.calendarOpen
                ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.38)
                : panel.capsuleBorder(0.38)
            Behavior on color { ColorAnimation { duration: Theme.motionFast } }
            Behavior on border.color { ColorAnimation { duration: Theme.motionFast } }
        }
        Row {
            anchors.centerIn: parent
            spacing: 7
            Text { text: clockShell.time; color: moduleForeground; font.family: Theme.font; font.pixelSize: 12; font.weight: Font.DemiBold }
            Rectangle { visible: panel.width > 1120; width: 1; height: 14; anchors.verticalCenter: parent.verticalCenter; color: panel.capsuleBorder(0.24) }
            Text { visible: panel.width > 1120; text: clockShell.date; color: Theme.muted; font.family: Theme.font; font.pixelSize: 11 }
        }
        MouseArea { id: clockMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: shell.calendarOpen = !shell.calendarOpen }
        Timer {
            interval: 1000
            repeat: true
            running: true
            triggeredOnStart: true
            onTriggered: {
                const now = new Date()
                clockShell.time = Qt.formatDateTime(now, Theme.clock24Hour ? "HH:mm" : "h:mm AP")
                clockShell.date = Qt.formatDateTime(now, "ddd, MM.dd")
            }
        }
    }
    MediaController { id: panelMedia; shell: panel.shell; polling: panel.visible && !panel.vertical }
    Rectangle {
        visible: !panel.vertical && panelMedia.media.available && width > 100
        anchors { right: statusShell.left; rightMargin: panel.capsuleGap; verticalCenter: parent.verticalCenter }
        width: Math.min(220, Math.max(0, statusShell.x - clockShell.x - clockShell.width - panel.capsuleGap * 2))
        height: panel.capsuleHeight
        radius: height / 2
        color: Theme.surfaceGlass
        border.width: 1
        border.color: Theme.hairline
        Text { anchors.fill: parent; anchors.margins: 10; text: panelMedia.media.title || shell.tr("Media"); color: Theme.text; font.family: Theme.font; font.pixelSize: 11; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter }
        MouseArea { anchors.fill: parent; onClicked: { shell.controlCenterTab = 1; shell.setAppearance({overview: !shell.overviewOpen}) } }
    }
    Item {
        id: verticalContent
        visible: panel.vertical
        anchors.fill: parent
        anchors.margins: 5

        Rectangle {
            anchors.fill: parent
            visible: panel.backgroundVisible
            radius: Math.min(width, 20)
            color: panel.capsuleColor(0.20, 0.58)
            border.width: 1
            border.color: panel.capsuleBorder(0.22)
        }

        Rectangle {
            id: verticalLauncher
            anchors.top: parent.top
            anchors.horizontalCenter: parent.horizontalCenter
            width: Math.max(30, parent.width - 6)
            height: width
            radius: width / 2
            color: Qt.rgba(panel.launcherAccent.r, panel.launcherAccent.g,
                           panel.launcherAccent.b, shell.launcherOpen ? 0.34 : 0.20)
            border.width: 1
            border.color: panel.launcherAccent
            LunaDashLogo {
                anchors.centerIn: parent
                width: parent.width * 0.72
                height: width
                animated: false
                primaryColor: panel.launcherAccent
                secondaryColor: Qt.lighter(panel.launcherAccent, 1.22)
                inkColor: Theme.text
            }
            MouseArea {
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: shell.openLauncherFromMouse()
            }
        }

        Column {
            id: verticalWorkspaces
            anchors.top: verticalLauncher.bottom
            anchors.topMargin: 7
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: 3
            Repeater {
                model: Math.max((shell.state.appearance || {}).workspaceCount || 10,
                                Number((shell.interaction || {}).workspace || 0) + 1)
                delegate: Rectangle {
                    required property int index
                    readonly property bool active:
                        Number((shell.interaction || {}).workspace ?? shell.state.workspace) === index
                    width: Math.max(24, verticalContent.width - 10)
                    height: active ? 26 : 20
                    radius: 8
                    color: active ? moduleAccent
                                  : Qt.rgba(moduleForeground.r, moduleForeground.g,
                                            moduleForeground.b, 0.12)
                    Text {
                        anchors.centerIn: parent
                        text: String(parent.index + 1)
                        color: parent.active ? Theme.accentInk : moduleForeground
                        font.family: Theme.font
                        font.pixelSize: 10
                        font.weight: parent.active ? Font.Bold : Font.Normal
                    }
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: shell.command("workspace", parent.index)
                    }
                }
            }
        }

        ListView {
            id: verticalTasks
            anchors.top: verticalWorkspaces.bottom
            anchors.topMargin: 7
            anchors.bottom: verticalStatus.top
            anchors.bottomMargin: 7
            anchors.left: parent.left
            anchors.right: parent.right
            orientation: ListView.Vertical
            spacing: 4
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            model: panel.groups.length
            delegate: Rectangle {
                id: verticalTask
                required property int index
                readonly property var group: panel.groups[index] || ({})
                readonly property var members: group.members || []
                width: verticalTasks.width
                height: Math.max(34, width)
                radius: 10
                color: group.active
                    ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.22)
                    : Qt.rgba(Theme.surfaceOpaque.r, Theme.surfaceOpaque.g,
                              Theme.surfaceOpaque.b, 0.62)
                border.width: 1
                border.color: group.active ? Theme.accent : panel.capsuleBorder(0.22)
                MemberIcon {
                    anchors.centerIn: parent
                    width: Math.max(20, Math.min(30, parent.width - 8))
                    height: width
                    shell: panel.shell
                    member: verticalTask.members.length ? verticalTask.members[0] : ({})
                }
                Rectangle {
                    visible: verticalTask.members.length > 1
                    anchors.right: parent.right
                    anchors.top: parent.top
                    width: 14
                    height: 14
                    radius: 7
                    color: Theme.accent
                    Text {
                        anchors.centerIn: parent
                        text: String(Math.min(9, verticalTask.members.length))
                        color: Theme.accentInk
                        font.family: Theme.font
                        font.pixelSize: 8
                        font.bold: true
                    }
                }
            }
        }

        Column {
            id: verticalStatus
            anchors.bottom: parent.bottom
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: 4

            Repeater {
                model: SystemTray.items
                delegate: Item {
                    required property var modelData
                    readonly property var item: modelData
                    visible: item.status !== Status.Passive
                    width: visible ? Math.max(26, verticalContent.width - 12) : 0
                    height: visible ? 28 : 0
                    readonly property string iconSource: panel.trayImage(item)
                    Image {
                        id: verticalTrayIcon
                        anchors.centerIn: parent
                        width: 18
                        height: 18
                        source: parent.iconSource
                        fillMode: Image.PreserveAspectFit
                        visible: parent.iconSource.length > 0 && status === Image.Ready
                    }
                    LineIcon {
                        anchors.centerIn: parent
                        width: 17
                        height: 17
                        visible: !verticalTrayIcon.visible
                        name: panel.trayGlyph(parent.item)
                        ink: Theme.text
                    }
                    MouseArea {
                        anchors.fill: parent
                        acceptedButtons: Qt.LeftButton | Qt.RightButton | Qt.MiddleButton
                        cursorShape: Qt.PointingHandCursor
                        onClicked: mouse => mouse.button === Qt.LeftButton
                            ? parent.item.activate() : parent.item.secondaryActivate()
                    }
                }
            }

            PanelSegment {
                moduleHost: panel
                width: Math.max(26, verticalContent.width - 12)
                height: 28
                text: "☷"
                fill: "transparent"
                border.width: 0
                Accessible.name: shell.tr("Clipboard")
                onClicked: shell.clipboardPopupOpen = !shell.clipboardPopupOpen
            }
            PanelSegment {
                moduleHost: panel
                width: Math.max(26, verticalContent.width - 12)
                height: 28
                text: panel.networkState.connected ? "◉" : "○"
                fill: "transparent"
                border.width: 0
                Accessible.name: panel.networkLabel()
                onClicked: shell.wifiPopupOpen = !shell.wifiPopupOpen
            }
            PanelSegment {
                moduleHost: panel
                width: Math.max(26, verticalContent.width - 12)
                height: 28
                text: "⚙"
                fill: "transparent"
                border.width: 0
                Accessible.name: shell.tr("Settings")
                onClicked: shell.settingsOpen = !shell.settingsOpen
            }
            Text {
                width: Math.max(26, verticalContent.width - 12)
                horizontalAlignment: Text.AlignHCenter
                color: moduleForeground
                font.family: Theme.font
                font.pixelSize: 10
                text: verticalClock.time
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: shell.calendarOpen = !shell.calendarOpen
                }
            }
            Item {
                id: verticalClock
                width: 1
                height: 1
                property string time: ""
                Timer {
                    interval: 1000
                    repeat: true
                    running: true
                    triggeredOnStart: true
                    onTriggered: verticalClock.time =
                        Qt.formatDateTime(new Date(), Theme.clock24Hour ? "HH:mm" : "h:mm")
                }
            }
        }
    }

}
