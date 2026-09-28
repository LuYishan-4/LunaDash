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
        24, moduleStyle.height || (shell.state.appearance || {}).panelHeight || 40)
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
    // Layer-shell adds the anchored edge margin to this reservation.
    exclusiveZone: thickness + moduleMargin
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
    readonly property real capsuleTint: Math.max(0, Math.min(100, panelConfig.capsuleTint ?? 12)) / 100.0
    readonly property int capsuleHeight: Math.max(16, Math.min(thickness - 8, (vertical ? height : width) / 8))
    readonly property int capsuleGap: 8
    readonly property int launcherExtent: Math.max(20, thickness - 2)
    readonly property bool centerLauncher: panelConfig.centerLauncher ?? true
    readonly property bool centeredLauncher: centerLauncher && width >= 700
    readonly property bool centeredClock: !centerLauncher && width >= 700
    readonly property string launcherImage: panelConfig.launcherImage || ""
    readonly property bool showSystemStats: panelConfig.showSystemStats ?? true
    readonly property bool showActiveTitle: panelConfig.showActiveTitle ?? true
    readonly property bool occupiedWorkspacesOnly: panelConfig.occupiedWorkspacesOnly ?? true
    readonly property int currentWorkspace: Number((shell.interaction || {}).workspace ?? shell.state.workspace ?? 0)
    readonly property var clients: (shell.interaction || {}).clients || shell.state.clients || []
    readonly property var activeClient: clients.find(client => client.focused && client.mapped && !client.desktop && !client.utility) || ({})
    readonly property var workspaceIds: WorkspaceTasks.visibleWorkspaces(
        clients, currentWorkspace, (shell.state.appearance || {}).workspaceCount || 10,
        occupiedWorkspacesOnly)
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
        const t = Math.max(0, Math.min(1, tintAmount)) * capsuleTint
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
        radius: Math.min(width, height) / 2
        color: panel.capsuleColor(0.20, 0.58)
        border.width: 1
        border.color: panel.capsuleBorder(0.22)
    }

    RowLayout {
        id: leftContent
        visible: !panel.vertical
        x: panel.centeredLauncher ? 4 : centerShell.x + centerShell.width + panel.capsuleGap
        y: (panel.height - panel.capsuleHeight) / 2
        width: Math.max(0, (panel.centeredLauncher ? centerShell.x : panel.centeredClock ? clockShell.x : statusShell.x) - x - panel.capsuleGap)
        height: panel.capsuleHeight
        spacing: panel.capsuleGap
        clip: true

        Rectangle {
            id: workspaceShell
            Layout.preferredWidth: Math.min(workspaceControls.contentWidth + 16, leftContent.width * (taskShell.visible ? 0.60 : 1))
            Layout.minimumWidth: 0
            Layout.fillHeight: true
            radius: height / 2
            color: panel.contrastShells ? panel.capsuleColor(0.72, 0.94) : "transparent"
            border.width: panel.contrastShells ? 1 : 0
            border.color: panel.capsuleBorder(0.40)
            ListView {
                id: workspaceControls
                anchors.fill: parent
                anchors.leftMargin: Math.min(8, parent.width / 4)
                anchors.rightMargin: Math.min(8, parent.width / 4)
                anchors.topMargin: Math.min(8, panel.capsuleHeight / 5)
                anchors.bottomMargin: Math.min(8, panel.capsuleHeight / 5)
                orientation: ListView.Horizontal
                spacing: 4
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                model: panel.workspaceIds
                onModelChanged: Qt.callLater(() => workspaceControls.positionViewAtIndex(panel.workspaceIds.indexOf(panel.currentWorkspace), ListView.Contain))
                delegate: Item {
                    id: workspacePill
                    required property int modelData
                    readonly property bool active: panel.currentWorkspace === modelData
                    width: active ? panel.workspaceActiveWidth : panel.workspaceInactiveWidth
                    height: workspaceControls.height
                    activeFocusOnTab: true
                    Accessible.role: Accessible.Button
                    Accessible.name: shell.tr("Workspace") + " " + (modelData + 1)
                    Accessible.onPressAction: shell.command("workspace", modelData)
                    Keys.onReturnPressed: shell.command("workspace", modelData)
                    Keys.onSpacePressed: shell.command("workspace", modelData)
                    Rectangle {
                        anchors.centerIn: parent
                        width: parent.width
                        height: panel.workspacePills ? Math.min(panel.workspacePillHeight, parent.height) : parent.height
                        radius: height / 2
                        color: workspacePill.active ? moduleAccent : Qt.rgba(moduleForeground.r, moduleForeground.g, moduleForeground.b, 0.18)
                        border.width: workspacePill.activeFocus ? 1 : 0
                        border.color: Theme.focusRing
                    }
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
            visible: panel.groups.length > 0 && leftContent.width >= 110
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
                        color: panel.contrastShells ? panel.capsuleColor(0.72, group.active ? 0.94 : 0.74) : "transparent"
                        border.width: panel.contrastShells ? 1 : 0
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
            color: panel.contrastShells ? panel.capsuleColor(0.85, 0.94) : "transparent"
            border.width: panel.contrastShells ? 1 : 0
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
                shell.toggleControlCenter(3);
            }
        }
        Item { Layout.fillWidth: true; Layout.minimumWidth: 0 }
    }

    PanelButton {
        id: centerShell
        visible: !panel.vertical
        moduleHost: panel
        x: panel.centeredLauncher ? (panel.width - width) / 2 : 4
        y: (panel.height - height) / 2
        width: panel.launcherExtent
        height: panel.launcherExtent
        Accessible.name: shell.tr("Applications")
        onClicked: shell.openLauncherFromMouse()
        LauncherMark {
            anchors.centerIn: parent
            width: parent.width * 0.94
            height: width
            source: panel.launcherImage
            accent: panel.launcherAccent
        }
    }

    Item {
        id: statusShell
        visible: !panel.vertical
        anchors.right: panel.centeredClock ? sessionButton.left : clockShell.left
        anchors.rightMargin: panel.capsuleGap
        anchors.verticalCenter: parent.verticalCenter
        height: panel.capsuleHeight
        width: Math.max(0, Math.min(statusControls.implicitWidth,
            (panel.centeredClock ? sessionButton.x : clockShell.x) - panel.capsuleGap
            - (panel.centeredClock ? clockShell.x + clockShell.width : centerShell.x + centerShell.width) - panel.capsuleGap))
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
                onClicked: shell.togglePopup("network")
            }
            PanelButton {
                visible: panel.width > 640
                moduleHost: panel
                iconName: "sound"
                label: panel.width > 1000 ? Math.round((((shell.state.audio || {}).output || {}).volume) || 0) + "%" : ""
                toolTip: shell.tr("Volume")
                selected: shell.volumePopupOpen
                onClicked: shell.togglePopup("audio")
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
                onClicked: shell.togglePopup("clipboard")
            }
            PanelButton {
                visible: panel.usbStorage.length > 0 && panel.width > 1100
                moduleHost: panel
                iconName: "usb"
                toolTip: shell.tr("USB devices")
                onClicked: shell.togglePopup("devices")
            }
            PanelButton {
                visible: panel.width > 1200 && (panel.stats.batteryPercent ?? -1) >= 0
                moduleHost: panel
                iconName: "power"
                label: panel.stats.batteryPercent + "%"
                toolTip: shell.tr("Battery")
                onClicked: shell.toggleSettingsPage("power")
            }
            PanelButton {
                moduleHost: panel
                iconName: "general"
                toolTip: shell.tr("Control center")
                selected: shell.overviewOpen
                onClicked: shell.toggleControlCenter(0)
            }
        }
    }

    PanelButton {
        id: clockShell
        visible: !panel.vertical
        moduleHost: panel
        x: panel.centeredClock ? (panel.width - width) / 2 : sessionButton.x - width - panel.capsuleGap
        y: (panel.height - height) / 2
        width: panel.centeredClock ? 150 : (Theme.clock24Hour ? 70 : 98)
        height: panel.capsuleHeight
        property string time: ""
        property string date: ""
        label: time + (panel.centeredClock ? "  " + date : "")
        toolTip: shell.tr("Calendar")
        selected: shell.calendarOpen
        onClicked: shell.togglePopup("calendar")
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
        anchors.rightMargin: 4
        anchors.verticalCenter: parent.verticalCenter
        width: panel.capsuleHeight
        height: panel.capsuleHeight
        iconName: "session"
        toolTip: shell.tr("Session controls")
        selected: shell.logoutOpen
        onClicked: shell.togglePopup("session")
    }
    ColumnLayout {
        id: verticalContent
        visible: panel.vertical
        anchors.fill: parent
        anchors.margins: 4
        spacing: 6
        clip: true

        PanelButton {
            moduleHost: panel
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: Math.min(panel.launcherExtent, verticalContent.width)
            Layout.preferredHeight: Math.min(panel.launcherExtent, verticalContent.width)
            Layout.minimumWidth: 0
            Layout.minimumHeight: 0
            Accessible.name: shell.tr("Applications")
            onClicked: shell.openLauncherFromMouse()
            LauncherMark {
                anchors.centerIn: parent
                width: parent.width * 0.94
                height: width
                source: panel.launcherImage
                accent: panel.launcherAccent
            }
        }

        ListView {
            id: verticalWorkspaces
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(contentHeight, Math.max(30, verticalContent.height * 0.24))
            Layout.minimumHeight: 0
            clip: true
            spacing: 3
            boundsBehavior: Flickable.StopAtBounds
            model: panel.workspaceIds
            onModelChanged: Qt.callLater(() => verticalWorkspaces.positionViewAtIndex(panel.workspaceIds.indexOf(panel.currentWorkspace), ListView.Contain))
            delegate: Rectangle {
                id: verticalWorkspace
                required property int modelData
                readonly property bool active: panel.currentWorkspace === modelData
                width: verticalWorkspaces.width
                height: 24
                radius: Math.min(width, height) / 2
                color: active ? moduleAccent : panel.contrastShells ? panel.capsuleColor(0.72, 0.94) : "transparent"
                activeFocusOnTab: true
                Accessible.role: Accessible.Button
                Accessible.name: shell.tr("Workspace") + " " + (modelData + 1)
                Accessible.onPressAction: shell.command("workspace", modelData)
                Keys.onReturnPressed: shell.command("workspace", modelData)
                Keys.onSpacePressed: shell.command("workspace", modelData)
                border.width: activeFocus ? 1 : 0
                border.color: Theme.focusRing
                Text {
                    anchors.fill: parent
                    text: panel.workspacePills ? "" : String(verticalWorkspace.modelData + 1)
                    color: verticalWorkspace.active ? Theme.accentInk : moduleForeground
                    font.family: Theme.font
                    font.pixelSize: 10
                    font.weight: verticalWorkspace.active ? Font.Bold : Font.Normal
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                }
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: shell.command("workspace", verticalWorkspace.modelData)
                }
            }
        }

        ListView {
            id: verticalTasks
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 0
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
                height: Math.max(24, Math.min(44, width))
                radius: Math.min(width, height) / 2
                color: panel.contrastShells ? panel.capsuleColor(0.72, 0.94) : "transparent"
                border.width: panel.contrastShells ? 1 : 0
                border.color: group.active ? Theme.accent : panel.capsuleBorder(0.22)
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: shell.command("workspace", verticalTask.group.workspace)
                }
                MemberIcon {
                    anchors.centerIn: parent
                    width: Math.max(12, Math.min(30, parent.width - 8))
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

        Flickable {
            visible: verticalContent.height > 480 && verticalTray.implicitHeight > 0
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(80, verticalTray.implicitHeight)
            Layout.minimumHeight: 0
            contentWidth: width
            contentHeight: verticalTray.implicitHeight
            flickableDirection: Flickable.VerticalFlick
            boundsBehavior: Flickable.StopAtBounds
            clip: true
            Column {
                id: verticalTray
                width: parent.width
                spacing: 4
                Repeater {
                    model: SystemTray.items
                    delegate: PanelButton {
                        id: verticalTrayButton
                        required property var modelData
                        moduleHost: panel
                        visible: modelData.status !== Status.Passive
                        width: Math.min(verticalTray.width, panel.capsuleHeight)
                        height: panel.capsuleHeight
                        x: (verticalTray.width - width) / 2
                        iconName: verticalTrayIcon.status === Image.Ready ? "" : panel.trayGlyph(modelData)
                        toolTip: modelData.tooltipTitle || modelData.title || modelData.id
                        onClicked: modelData.activate()
                        Image {
                            id: verticalTrayIcon
                            anchors.centerIn: parent
                            width: Math.min(18, parent.width)
                            height: width
                            source: panel.trayImage(verticalTrayButton.modelData)
                            fillMode: Image.PreserveAspectFit
                            visible: status === Image.Ready
                        }
                        MouseArea {
                            anchors.fill: parent
                            acceptedButtons: Qt.RightButton | Qt.MiddleButton
                            onClicked: verticalTrayButton.modelData.secondaryActivate()
                            onWheel: wheel => verticalTrayButton.modelData.scroll(wheel.angleDelta.y || wheel.angleDelta.x, wheel.angleDelta.x !== 0)
                        }
                    }
                }
            }
        }
        PanelButton {
            visible: verticalContent.height > 540
            moduleHost: panel
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: panel.capsuleHeight
            Layout.preferredHeight: panel.capsuleHeight
            Layout.minimumWidth: 0
            Layout.minimumHeight: 0
            iconName: panel.networkState.ethernetConnected ? "ethernet" : panel.networkState.connected ? "network" : "network-off"
            toolTip: panel.networkLabel()
            selected: shell.wifiPopupOpen
            onClicked: shell.togglePopup("network")
        }
        PanelButton {
            visible: verticalContent.height > 440
            moduleHost: panel
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: panel.capsuleHeight
            Layout.preferredHeight: panel.capsuleHeight
            Layout.minimumWidth: 0
            Layout.minimumHeight: 0
            iconName: "sound"
            toolTip: shell.tr("Volume")
            selected: shell.volumePopupOpen
            onClicked: shell.togglePopup("audio")
        }
        PanelButton {
            moduleHost: panel
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: panel.capsuleHeight
            Layout.preferredHeight: panel.capsuleHeight
            Layout.minimumWidth: 0
            Layout.minimumHeight: 0
            iconName: "general"
            toolTip: shell.tr("Control center")
            selected: shell.overviewOpen
            onClicked: shell.toggleControlCenter(0)
        }
        PanelButton {
            id: verticalClock
            moduleHost: panel
            Layout.fillWidth: true
            Layout.preferredHeight: panel.thickness < 64 ? 34 : 26
            Layout.minimumWidth: 0
            Layout.minimumHeight: 0
            property string time: ""
            label: panel.thickness < 64 ? time.replace(":", "\n") : time
            toolTip: shell.tr("Calendar") + " " + time
            selected: shell.calendarOpen
            onClicked: shell.togglePopup("calendar")
            Timer {
                interval: 1000
                repeat: true
                running: panel.vertical
                triggeredOnStart: true
                onTriggered: verticalClock.time = Qt.formatDateTime(new Date(), Theme.clock24Hour ? "HH:mm" : "h:mm")
            }
        }
        PanelButton {
            moduleHost: panel
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: panel.capsuleHeight
            Layout.preferredHeight: panel.capsuleHeight
            Layout.minimumWidth: 0
            Layout.minimumHeight: 0
            iconName: "session"
            toolTip: shell.tr("Session controls")
            selected: shell.logoutOpen
        onClicked: shell.togglePopup("session")
        }
    }
}
