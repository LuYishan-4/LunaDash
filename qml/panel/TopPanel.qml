import "../modules"
import "../plugins"
import QtQuick
import "../columns/WorkspaceTasks.js" as WorkspaceTasks
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import Quickshell
import Quickshell.Services.SystemTray
import Quickshell.Wayland
import Quickshell.Widgets
import "../columns"
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
        top: moduleMargin
        bottom: moduleMargin
        left: moduleMargin
        right: moduleMargin
    }

    implicitWidth: vertical
        ? thickness
        : Math.max(1, Math.min(requestedLength || (screen ? screen.width : 1440),
                              screen ? screen.width - 2 * moduleMargin : 3840))
    implicitHeight: vertical
        ? Math.max(1, Math.min(requestedLength || (screen ? screen.height : 900),
                              screen ? screen.height - 2 * moduleMargin : 2160))
        : thickness
    exclusiveZone: (vertical ? implicitWidth : implicitHeight) + 6
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
    readonly property real shellOpacity: Math.max(0, Math.min(60, panelConfig.shellOpacity ?? 20)) / 100.0
    readonly property int capsuleHeight: Math.max(30, implicitHeight - 8)
    readonly property int capsuleGap: 8
    readonly property bool centerLauncher: panelConfig.centerLauncher ?? true
    readonly property bool showSystemStats: panelConfig.showSystemStats ?? true
    readonly property bool showActiveTitle: panelConfig.showActiveTitle ?? true
    readonly property bool occupiedWorkspacesOnly: panelConfig.occupiedWorkspacesOnly ?? true
    readonly property int currentWorkspace: Number((shell.interaction || {}).workspace ?? shell.state.workspace ?? 0)
    readonly property var clients: (shell.interaction || {}).clients || shell.state.clients || []
    readonly property var activeClient: clients.find(client => client.focused && client.mapped && !client.desktop && !client.utility) || ({})
    readonly property var workspaceIds: {
        const count = Math.max((shell.state.appearance || {}).workspaceCount || 10, currentWorkspace + 1)
        if (!occupiedWorkspacesOnly)
            return Array.from({length: count}, (_, index) => index)
        const occupied = new Set(groups.map(group => group.workspace))
        occupied.add(currentWorkspace)
        // Keep one empty destination available without changing workspace capacity.
        for (let index = 0; index < count; ++index) {
            if (!occupied.has(index)) { occupied.add(index); break }
        }
        return Array.from(occupied).sort((a, b) => a - b)
    }
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
        // Preserve the established surface at the registry's default (20),
        // while the existing 0–60 control spans transparent through opaque.
        const baseOpacity = Math.min(alpha, 0.84)
        const opacity = shellOpacity <= 0.20
            ? baseOpacity * shellOpacity / 0.20
            : baseOpacity + (1 - baseOpacity) * (shellOpacity - 0.20) / 0.40
        return Qt.rgba(
            moduleBackground.r * (1 - t) + Theme.secondaryAccent.r * t,
            moduleBackground.g * (1 - t) + Theme.secondaryAccent.g * t,
            moduleBackground.b * (1 - t) + Theme.secondaryAccent.b * t,
            Theme.eyeCare ? 1 : opacity
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

    RowLayout {
        id: leftContent
        visible: !panel.vertical
        x: panel.centerLauncher ? 8 : centerShell.x + centerShell.width + panel.capsuleGap
        y: (panel.height - panel.capsuleHeight) / 2
        width: Math.max(0, (panel.centerLauncher ? centerShell.x : clockShell.x) - x - panel.capsuleGap)
        height: panel.capsuleHeight
        spacing: panel.capsuleGap

        Rectangle {
            id: workspaceShell
            Layout.preferredWidth: Math.min(workspaceControls.contentWidth + 16, leftContent.width * 0.48)
            Layout.minimumWidth: 0
            Layout.fillHeight: true
            radius: height / 2
            color: panel.contrastShells ? panel.capsuleColor(0.72, 0.94) : "transparent"
            border.width: panel.contrastShells ? 1 : 0
            border.color: panel.capsuleBorder(0.40)
            ListView {
                id: workspaceControls
                anchors.fill: parent
                anchors.margins: 8
                orientation: ListView.Horizontal
                spacing: 4
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                model: panel.workspaceIds
                onModelChanged: Qt.callLater(() => workspaceControls.positionViewAtIndex(panel.workspaceIds.indexOf(panel.currentWorkspace), ListView.Contain))
                delegate: Rectangle {
                    id: workspacePill
                    required property int modelData
                    readonly property bool active: panel.currentWorkspace === modelData
                    width: active ? panel.workspaceActiveWidth : panel.workspaceInactiveWidth
                    height: panel.workspacePills ? Math.min(panel.workspacePillHeight, workspaceControls.height) : workspaceControls.height
                    y: (workspaceControls.height - height) / 2
                    radius: height / 2
                    color: active ? moduleAccent : Qt.rgba(moduleForeground.r, moduleForeground.g, moduleForeground.b, 0.18)
                    activeFocusOnTab: true
                    Accessible.role: Accessible.Button
                    Accessible.name: shell.tr("Workspace") + " " + (modelData + 1)
                    Keys.onReturnPressed: shell.command("workspace", modelData)
                    Keys.onSpacePressed: shell.command("workspace", modelData)
                    border.width: activeFocus ? 1 : 0
                    border.color: Theme.focusRing
                    Text {
                        anchors.fill: parent
                        text: panel.workspacePills ? "" : String(workspacePill.modelData + 1)
                        color: workspacePill.active ? Theme.accentInk : Theme.text
                        font.family: Theme.font
                        font.pixelSize: 10
                        font.weight: Font.DemiBold
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        elide: Text.ElideRight
                    }
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: shell.command("workspace", workspacePill.modelData)
                    }
                }
            }
        }

        Item {
            id: taskShell
            visible: panel.groups.length > 0
            Layout.preferredWidth: Math.min(columnTasks.contentWidth + 8, leftContent.width * 0.32)
            Layout.minimumWidth: 0
            Layout.fillHeight: true
            ExtensionSlot {
                anchors.fill: parent
                shell: panel.shell
                target: "taskbar-windows"
                context: ({groups: panel.groups})
                ListView {
                    id: columnTasks
                    anchors.fill: parent
                    anchors.margins: 2
                    orientation: ListView.Horizontal
                    spacing: 4
                    clip: true
                    boundsBehavior: Flickable.StopAtBounds
                    model: panel.groups.length
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

        Rectangle {
            visible: panel.showActiveTitle && panel.width > 900 && Boolean(panel.activeClient.title)
            Layout.fillWidth: true
            Layout.maximumWidth: 260
            Layout.minimumWidth: 0
            Layout.fillHeight: true
            radius: height / 2
            color: panel.capsuleColor(0.85, 0.94)
            border.width: 1
            border.color: Theme.hairline
            Text {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                text: panel.activeClient.title || ""
                textFormat: Text.PlainText
                color: moduleAccent
                font.family: Theme.font
                font.pixelSize: 12
                font.weight: Font.DemiBold
                elide: Text.ElideRight
                verticalAlignment: Text.AlignVCenter
            }
        }

        PanelButton {
            visible: panel.showSystemStats && panel.width > 1100
            moduleHost: panel
            Layout.preferredWidth: 132
            Layout.fillHeight: true
            iconName: "monitor"
            label: Math.round(panel.stats.cpuPercent || 0) + "%  " + Number(panel.stats.memoryUsed || 0).toFixed(1) + " GiB"
            toolTip: shell.tr("System monitor")
            onClicked: {
                shell.controlCenterTab = 3;
                shell.setAppearance({overview: true});
            }
        }
        Item { Layout.fillWidth: true; Layout.minimumWidth: 0 }
    }

    PanelButton {
        id: centerShell
        visible: !panel.vertical
        moduleHost: panel
        x: panel.centerLauncher ? (panel.width - width) / 2 : 8
        y: (panel.height - height) / 2
        width: panel.capsuleHeight
        height: panel.capsuleHeight
        iconName: "apps"
        toolTip: shell.tr("Applications")
        onClicked: shell.openLauncherFromMouse()
    }

    Item {
        id: statusShell
        visible: !panel.vertical
        anchors.right: panel.centerLauncher ? clockShell.left : sessionButton.left
        anchors.rightMargin: panel.capsuleGap
        anchors.verticalCenter: parent.verticalCenter
        height: panel.capsuleHeight
        width: Math.max(0, Math.min(statusControls.implicitWidth,
            (panel.centerLauncher ? clockShell.x : sessionButton.x) - panel.capsuleGap
            - (panel.centerLauncher ? centerShell.x + centerShell.width : clockShell.x + clockShell.width) - panel.capsuleGap))
        clip: true
        Row {
            id: statusControls
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            spacing: 5
            // Tray inventory is bounded independently so system controls remain reachable.
            Flickable {
                visible: panel.width > 900
                width: visible ? Math.min(150, trayRow.implicitWidth) : 0
                height: panel.capsuleHeight
                contentWidth: trayRow.implicitWidth
                contentHeight: height
                flickableDirection: Flickable.HorizontalFlick
                boundsBehavior: Flickable.StopAtBounds
                clip: true
                Row {
                    id: trayRow
                    spacing: 3
                    Repeater {
                        model: SystemTray.items
                        delegate: PanelButton {
                            id: trayDelegate
                            required property var modelData
                            moduleHost: panel
                            visible: modelData.status !== Status.Passive
                            width: visible ? panel.capsuleHeight : 0
                            height: panel.capsuleHeight
                            iconName: trayImage.status === Image.Ready ? "" : panel.trayGlyph(modelData)
                            toolTip: modelData.tooltipTitle || modelData.title || modelData.id
                            onClicked: modelData.activate()
                            Image {
                                id: trayImage
                                anchors.centerIn: parent
                                width: 18
                                height: 18
                                source: panel.trayImage(trayDelegate.modelData)
                                sourceSize.width: Math.max(1, Math.round(width * Screen.devicePixelRatio))
                                sourceSize.height: Math.max(1, Math.round(height * Screen.devicePixelRatio))
                                fillMode: Image.PreserveAspectFit
                                visible: status === Image.Ready
                            }
                            MouseArea {
                                anchors.fill: parent
                                acceptedButtons: Qt.RightButton | Qt.MiddleButton
                                onClicked: trayDelegate.modelData.secondaryActivate()
                                onWheel: wheel => trayDelegate.modelData.scroll(wheel.angleDelta.y || wheel.angleDelta.x, wheel.angleDelta.x !== 0)
                            }
                        }
                    }
                }
            }
            PanelButton {
                visible: panel.width > 760
                moduleHost: panel
                iconName: panel.networkState.ethernetConnected ? "ethernet" : panel.networkState.connected ? "network" : "network-off"
                toolTip: panel.networkLabel()
                selected: shell.wifiPopupOpen
                onClicked: shell.wifiPopupOpen = !shell.wifiPopupOpen
            }
            PanelButton {
                visible: panel.width > 640
                moduleHost: panel
                iconName: "sound"
                label: panel.width > 1000 ? Math.round((((shell.state.audio || {}).output || {}).volume) || 0) + "%" : ""
                toolTip: shell.tr("Volume")
                selected: shell.volumePopupOpen
                onClicked: shell.volumePopupOpen = !shell.volumePopupOpen
                onScrolled: delta => {
                    const current = Number((((shell.state.audio || {}).output || {}).volume) || 0)
                    shell.command("audio", JSON.stringify({device: "output", volume: Math.max(0, Math.min(100, current + (delta > 0 ? 5 : -5)))}))
                }
            }
            PanelButton {
                visible: panel.width > 1000
                moduleHost: panel
                iconName: "copy"
                toolTip: shell.tr("Clipboard")
                selected: shell.clipboardPopupOpen
                onClicked: shell.clipboardPopupOpen = !shell.clipboardPopupOpen
            }
            PanelButton {
                visible: panel.usbStorage.length > 0 && panel.width > 1100
                moduleHost: panel
                iconName: "usb"
                toolTip: shell.tr("USB devices")
                onClicked: shell.usbPopupOpen = !shell.usbPopupOpen
            }
            PanelButton {
                visible: panel.width > 1200 && (panel.stats.batteryPercent ?? -1) >= 0
                moduleHost: panel
                iconName: "power"
                label: panel.stats.batteryPercent + "%"
                toolTip: shell.tr("Battery")
                onClicked: shell.openSettingsPage("power")
            }
            PanelButton {
                moduleHost: panel
                iconName: "general"
                toolTip: shell.tr("Control center")
                selected: shell.overviewOpen
                onClicked: { shell.controlCenterTab = 0; shell.setAppearance({overview: !shell.overviewOpen}) }
            }
        }
    }

    PanelButton {
        id: clockShell
        visible: !panel.vertical
        moduleHost: panel
        x: panel.centerLauncher ? sessionButton.x - width - panel.capsuleGap : (panel.width - width) / 2
        y: (panel.height - height) / 2
        width: panel.centerLauncher ? (Theme.clock24Hour ? 70 : 98) : 150
        height: panel.capsuleHeight
        property string time: ""
        property string date: ""
        label: time + (panel.centerLauncher ? "" : "  " + date)
        toolTip: shell.tr("Calendar")
        selected: shell.calendarOpen
        onClicked: shell.calendarOpen = !shell.calendarOpen
        Timer {
            interval: 1000
            repeat: true
            running: !panel.vertical
            triggeredOnStart: true
            onTriggered: {
                const now = new Date()
                clockShell.time = Qt.formatDateTime(now, Theme.clock24Hour ? "HH:mm" : "h:mm AP")
                clockShell.date = Qt.formatDateTime(now, "ddd, MM.dd")
            }
        }
    }
    PanelButton {
        id: sessionButton
        visible: !panel.vertical
        moduleHost: panel
        anchors.right: parent.right
        anchors.rightMargin: 8
        anchors.verticalCenter: parent.verticalCenter
        width: panel.capsuleHeight
        height: panel.capsuleHeight
        iconName: "session"
        toolTip: shell.tr("Session controls")
        onClicked: shell.logoutOpen = true
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
                model: panel.workspaceIds
                delegate: Rectangle {
                    required property int modelData
                    readonly property bool active: panel.currentWorkspace === modelData
                    width: Math.max(24, verticalContent.width - 10)
                    height: active ? 26 : 20
                    radius: 8
                    color: active ? moduleAccent
                                  : Qt.rgba(moduleForeground.r, moduleForeground.g,
                                            moduleForeground.b, 0.12)
                    Text {
                        anchors.centerIn: parent
                        text: String(parent.modelData + 1)
                        color: parent.active ? Theme.accentInk : moduleForeground
                        font.family: Theme.font
                        font.pixelSize: 10
                        font.weight: parent.active ? Font.Bold : Font.Normal
                    }
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: shell.command("workspace", parent.modelData)
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
