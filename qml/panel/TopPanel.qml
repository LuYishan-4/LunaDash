import "../modules"
import QtQuick
import QtQuick.Controls
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
    anchors { top: moduleStyle.edge !== "bottom"; bottom: moduleStyle.edge === "bottom"; left: !moduleStyle.width; right: !moduleStyle.width }
    margins { top: moduleMargin; bottom: moduleMargin; left: moduleMargin; right: moduleMargin }
    implicitWidth: moduleWidth(1440)
    implicitHeight: moduleHeight((shell.state.appearance || {}).panelHeight || 40)
    exclusiveZone: implicitHeight + moduleMargin * 2
    color: "transparent"
    WlrLayershell.namespace: "lunadash-panel"

    property var stats: shell.state.system || ({})
    readonly property var groups: ((shell.state.tiling || {}).groups || [])
    readonly property int focusedGroupIndex: {
        for (let index = 0; index < groups.length; ++index)
            if (groups[index].focused) return index
        return -1
    }
    onFocusedGroupIndexChanged: if (focusedGroupIndex >= 0) columnTasks.positionViewAtIndex(focusedGroupIndex, ListView.Contain)
    onGroupsChanged: Qt.callLater(() => { if (focusedGroupIndex >= 0) columnTasks.positionViewAtIndex(focusedGroupIndex, ListView.Contain) })

    function trayImage(item) {
        const supplied = String(item.icon || "")
        if (/^(image:|file:|qrc:|data:)/.test(supplied) && !supplied.includes("qs-blackhole")) return supplied
        if (supplied.startsWith("/") && /\.(png|jpe?g|webp|svg|xpm)$/i.test(supplied)) return "file://" + supplied
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

    Rectangle {
        anchors.fill: parent
        radius: Math.min(8, height / 2)
        color: moduleBackground
        border.width: 1
        border.color: Qt.rgba(moduleAccent.r, moduleAccent.g, moduleAccent.b, 0.28)
    }

    Row {
        id: workspaceControls
        anchors { left: parent.left; leftMargin: 8; verticalCenter: parent.verticalCenter }
        spacing: 3
        Repeater {
            model: (shell.state.appearance || {}).workspaceCount || 4
            PanelSegment {
                moduleHost: panel
                required property int index
                text: String(index + 1)
                implicitWidth: panel.moduleWidth(shell.state.workspace === index ? 32 : 24)
                selected: shell.state.workspace === index
                onClicked: shell.command("workspace", index)
            }
        }
        PanelSegment {
            moduleHost: panel
            text: "⏻"
            implicitWidth: 26
            ink: Theme.danger
            Accessible.name: shell.tr("Session controls")
            onClicked: shell.logoutOpen = !shell.logoutOpen
        }
    }

    Row {
        id: centerSelector
        anchors.centerIn: parent
        height: Math.max(28, panel.height - 6)
        spacing: 2

        PanelSegment {
            moduleHost: panel
            text: "◈"
            implicitWidth: 28
            Accessible.name: shell.tr("Overview")
            onClicked: shell.setAppearance({ overview: !shell.overviewOpen })
        }
        Rectangle {
            width: 54
            height: centerSelector.height
            radius: 4
            color: Theme.accent
            border.width: 1
            border.color: Qt.lighter(Theme.accent, 1.18)
            Text {
                anchors.centerIn: parent
                text: "Λ"
                color: Theme.accentInk
                font.pixelSize: 20
                font.bold: true
            }
            MouseArea {
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: shell.launcherOpen = !shell.launcherOpen
            }
            Behavior on color { ColorAnimation { duration: Theme.motion } }
        }
        PanelSegment {
            moduleHost: panel
            text: "⚙"
            implicitWidth: 28
            Accessible.name: shell.tr("Settings")
            onClicked: shell.settingsOpen = !shell.settingsOpen
        }
    }

    ListView {
        id: columnTasks
        anchors { left: workspaceControls.right; leftMargin: 6; right: centerSelector.left; rightMargin: 8; verticalCenter: parent.verticalCenter }
        height: Math.max(28, panel.height - 8)
        orientation: ListView.Horizontal
        spacing: 3
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        model: panel.groups
        visible: count > 0
        Behavior on contentX {
            enabled: !columnTasks.flicking && !columnTasks.moving
            NumberAnimation { duration: Math.max(120, Theme.motion); easing.type: Easing.OutCubic }
        }
        delegate: ColumnCell {
            required property var modelData
            shell: panel.shell
            group: modelData
            height: columnTasks.height
            width: Math.max(38, Math.min(142, ((modelData.members || []).length * 30) + 12))
        }
    }

    Row {
        anchors { right: parent.right; rightMargin: 8; verticalCenter: parent.verticalCenter }
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
                    radius: 5
                    color: trayMouse.containsMouse ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.16) : "transparent"
                }
                readonly property string iconSource: panel.trayImage(item)
                readonly property string glyph: panel.trayGlyph(item)
                // A plain Image with a bounded sourceSize rasterizes an SVG icon at
                // exactly the requested size. Asking for the icon's intrinsic size
                // made QtSvg reject oversized masks and left undecoded pixels in the
                // tray cell. When the supplied icon cannot be resolved the shell
                // draws its own vector glyph instead of a broken or empty cell.
                Image {
                    id: trayIcon
                    anchors.centerIn: parent
                    width: Math.max(10, Math.min(19, parent.width - 4))
                    height: width
                    source: trayDelegate.iconSource
                    sourceSize.width: Math.max(1, Math.round(width * Screen.devicePixelRatio))
                    sourceSize.height: Math.max(1, Math.round(height * Screen.devicePixelRatio))
                    fillMode: Image.PreserveAspectFit
                    smooth: true
                    mipmap: true
                    visible: trayDelegate.iconSource.length > 0 && status === Image.Ready
                }
                LineIcon {
                    anchors.centerIn: parent
                    width: 17
                    height: 17
                    visible: !trayIcon.visible
                    name: trayDelegate.glyph
                    ink: Theme.text
                }
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

        PanelSegment {
            moduleHost: panel
            id: clock
            property string time: ""
            text: time
            implicitWidth: panel.moduleWidth(panel.width > 850 ? 76 : 52)
            onClicked: shell.setAppearance({ overview: !shell.overviewOpen })
            Timer {
                interval: 1000
                repeat: true
                running: true
                triggeredOnStart: true
                onTriggered: clock.time = Qt.formatDateTime(new Date(), panel.width > 850 ? (Theme.clock24Hour ? "HH:mm:ss" : "h:mm AP") : (Theme.clock24Hour ? "HH:mm" : "h:mm"))
            }
        }
        PanelSegment {
            moduleHost: panel
            visible: panel.width > 720
            text: (shell.state.network || {}).connected ? "↔" : "×"
            ink: (shell.state.network || {}).internet ? Theme.accent : Theme.muted
            Accessible.name: shell.tr("Network")
            onClicked: shell.settingsOpen = !shell.settingsOpen
        }
        PanelSegment {
            moduleHost: panel
            visible: panel.width > 860 && (panel.stats.batteryPercent ?? -1) >= 0
            text: panel.stats.batteryPercent + "%"
            Accessible.name: shell.tr("Battery")
            onClicked: shell.launch("monitor")
        }
    }
}
