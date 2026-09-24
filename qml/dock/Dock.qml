import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Quickshell
import Quickshell.Wayland
import "../modules"
import "../components"
import "../style"

ModuleSurface {
    id: dock
    moduleId: "dock"
    anchors.bottom: true
    margins.bottom: Theme.panelBottomInset + moduleMargin
    implicitWidth: moduleWidth(Math.min(720, Math.max(240, content.implicitWidth + 24)))
    implicitHeight: expanded ? 68 : 8
    exclusionMode: ExclusionMode.Ignore
    WlrLayershell.layer: WlrLayer.Top
    WlrLayershell.namespace: "lunadash-dock"
    color: "transparent"
    readonly property var clients: (shell.state.clients || []).filter(client => client.mapped && !client.desktop && !client.utility)
    readonly property bool autoHide: (shell.state.appearance || {}).dockAutoHide ?? true
    readonly property bool occupied: clients.some(client => !client.minimized && client.workspace === shell.state.workspace)
    readonly property bool expanded: !autoHide || !occupied || hover.hovered
    readonly property var pins: (specification.config || {}).pins || ["terminal", "files", "browser"]
    HoverHandler {
        id: hover
    }

    Rectangle {
        anchors.fill: parent
        radius: Theme.radius
        color: Theme.surfaceGlass
        border.width: 1
        border.color: Theme.hairline
        clip: true
        RowLayout {
            id: content
            anchors.fill: parent
            anchors.margins: 12
            spacing: 6
            visible: dock.expanded
            ShellButton {
                iconName: "apps"
                toolTip: shell.tr("Orbit launcher")
                Accessible.name: toolTip
                onClicked: shell.orbitOpen = !shell.orbitOpen
            }
            Repeater {
                model: dock.pins
                ShellButton {
                    required property string modelData
                    iconName: modelData === "files" ? "files" : modelData === "terminal" ? "terminal" : "search"
                    toolTip: shell.tr(modelData === "files" ? "Files" : modelData === "terminal" ? "Terminal" : "Browser")
                    Accessible.name: toolTip
                    onClicked: shell.launch(modelData)
                }
            }
            Rectangle {
                implicitWidth: 1
                implicitHeight: 28
                color: Theme.hairline
                visible: dock.clients.length > 0
            }
            ListView {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                Layout.preferredWidth: Math.min(280, dock.clients.length * 50)
                Layout.preferredHeight: 50
                orientation: ListView.Horizontal
                model: dock.clients
                spacing: 4
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                delegate: Item {
                    required property var modelData
                    width: 46
                    height: 50
                    Accessible.role: Accessible.Button
                    Accessible.name: modelData.title || modelData.appId || shell.tr("Application")
                    activeFocusOnTab: true
                    Keys.onReturnPressed: shell.command("activate-window", modelData.id)
                    Rectangle {
                        anchors.fill: parent
                        anchors.margins: 3
                        radius: Theme.radiusSmall
                        color: taskHover.containsMouse ? Theme.surfaceHover : "transparent"
                    }
                    ApplicationIcon {
                        anchors.centerIn: parent
                        width: 30
                        height: 30
                        shell: dock.shell
                        appId: modelData.appId || ""
                        title: modelData.title || ""
                        iconName: modelData.icon || ""
                    }
                    Rectangle {
                        anchors.bottom: parent.bottom
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: modelData.focused ? 14 : 4
                        height: 3
                        radius: 2
                        color: Theme.accent
                        opacity: modelData.minimized ? 0.45 : 1
                    }
                    MouseArea {
                        id: taskHover
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: shell.command("activate-window", modelData.id)
                    }
                    ToolTip.visible: taskHover.containsMouse
                    ToolTip.text: modelData.title || modelData.appId || ""
                }
            }
        }
    }
}
