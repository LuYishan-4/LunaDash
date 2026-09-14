import "../modules"
import QtQuick
import QtQuick.Controls
import Quickshell
import Quickshell.Services.SystemTray
import Quickshell.Wayland
import Quickshell.Widgets
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
    WlrLayershell.namespace: "lunadah-panel"
    property var stats: shell.state.system || ({})

    Rectangle { anchors.fill: parent; color: moduleBackground; radius: moduleRadius }

    Row {
        anchors { left: parent.left; leftMargin: 10; verticalCenter: parent.verticalCenter }
        spacing: 5
        Repeater {
            model: (shell.state.appearance || {}).workspaceCount || 4
            PanelSegment {
                moduleHost: panel
                required property int index
                text: String(index + 1)
                implicitWidth: panel.moduleWidth(panel.width < 1100 ? 24 : panel.shell.state.workspace === index ? 44 : 30)
                selected: shell.state.workspace === index
                onClicked: shell.command("workspace", index)
            }
        }
        PanelSegment {
            moduleHost: panel
            text: "⏻"
            ink: Theme.danger
            Accessible.name: shell.tr("Session controls")
            onClicked: shell.logoutOpen = !shell.logoutOpen
        }
    }

    Item {
        anchors.centerIn: parent
        width: 44
        height: Math.max(28, panel.height - 6)
        BrandIcon { shell: panel.shell; anchors.centerIn: parent; width: 34; height: 26 }
        MouseArea {
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: shell.launcherOpen = !shell.launcherOpen
        }
    }

    Row {
        anchors { right: parent.right; rightMargin: 10; verticalCenter: parent.verticalCenter }
        spacing: 4

        Repeater {
            model: SystemTray.items
            delegate: Item {
                id: trayDelegate
                required property var modelData
                readonly property var item: modelData
                visible: item.status !== Status.Passive
                width: visible ? Math.min(30, panel.height - 6) : 0
                height: Math.min(30, panel.height - 6)

                IconImage {
                    anchors.centerIn: parent
                    width: Math.min(20, parent.width)
                    height: width
                    source: trayDelegate.item.icon
                }
                MouseArea {
                    id: trayMouse
                    anchors.fill: parent
                    acceptedButtons: Qt.LeftButton | Qt.RightButton | Qt.MiddleButton
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: mouse => {
                        if (mouse.button === Qt.LeftButton)
                            trayDelegate.item.activate()
                        else
                            trayDelegate.item.secondaryActivate()
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
            onClicked: shell.setAppearance({ overview: !shell.overviewOpen })
            Timer {
                interval: 1000
                repeat: true
                running: true
                triggeredOnStart: true
                onTriggered: clock.time = Qt.formatDateTime(new Date(), panel.width > 850 ? (Theme.clock24Hour ? "ddd  HH:mm" : "ddd  h:mm AP") : (Theme.clock24Hour ? "HH:mm" : "h:mm AP"))
            }
        }
        PanelSegment {
            moduleHost: panel
            visible: panel.width > 620
            text: (shell.state.network || {}).connected ? "↔" : "×"
            ink: (shell.state.network || {}).internet ? Theme.accent : Theme.muted
            Accessible.name: shell.tr("Network")
            onClicked: shell.settingsOpen = !shell.settingsOpen
        }
        PanelSegment {
            moduleHost: panel
            visible: panel.width > 760 && (panel.stats.batteryPercent ?? -1) >= 0
            text: panel.stats.batteryPercent + "%"
            Accessible.name: shell.tr("Battery")
            onClicked: shell.launch("monitor")
        }
    }
}
