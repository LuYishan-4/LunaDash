import "../modules"
import QtQuick
import QtQuick.Layouts
import Quickshell
import Quickshell.Wayland
import "../components"
import "../style"

ModuleSurface {
    id: menu
    moduleId: "session"
    property string pendingAction: ""
    property var availability: shell.state.sessionActions || {}

    anchors { top: true; right: true }
    margins { top: Theme.barHeight + moduleMargin; right: moduleMargin }
    implicitWidth: moduleWidth(300)
    implicitHeight: moduleHeight(pendingAction === "" ? 292 : 170)
    exclusionMode: ExclusionMode.Ignore
    WlrLayershell.layer: WlrLayer.Overlay
    WlrLayershell.namespace: "lunadah-logout"
    color: "transparent"

    Rectangle { anchors.fill: parent; radius: moduleRadius; color: moduleBackground; border.color: Theme.border }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 18
        spacing: 9

        Text {
            Layout.fillWidth: true
            text: menu.pendingAction === "reboot" ? shell.tr("Restart this computer?")
                  : menu.pendingAction === "poweroff" ? shell.tr("Shut down this computer?")
                  : shell.tr("Session")
            color: moduleForeground
            font.family: Theme.font
            font.pixelSize: 14
            wrapMode: Text.WordWrap
        }

        ColumnLayout {
            visible: menu.pendingAction === ""
            Layout.fillWidth: true
            spacing: 7
            ShellButton { Layout.fillWidth: true; text: shell.tr("Log out"); onClicked: { shell.logoutOpen = false; shell.command("quit", "") } }
            ShellButton { Layout.fillWidth: true; text: shell.tr("Suspend"); enabled: menu.availability.suspend === true; onClicked: { shell.logoutOpen = false; shell.command("session-action", "suspend") } }
            ShellButton { Layout.fillWidth: true; text: shell.tr("Restart"); enabled: menu.availability.reboot === true; onClicked: menu.pendingAction = "reboot" }
            ShellButton { Layout.fillWidth: true; text: shell.tr("Shut down"); enabled: menu.availability.poweroff === true; onClicked: menu.pendingAction = "poweroff" }
            ShellButton { Layout.fillWidth: true; text: shell.tr("Cancel"); onClicked: shell.logoutOpen = false }
        }

        RowLayout {
            visible: menu.pendingAction !== ""
            Layout.fillWidth: true
            ShellButton { Layout.fillWidth: true; text: shell.tr("Cancel"); onClicked: menu.pendingAction = "" }
            ShellButton {
                Layout.fillWidth: true
                text: menu.pendingAction === "reboot" ? shell.tr("Confirm restart") : shell.tr("Confirm shut down")
                active: true
                onClicked: {
                    const action = menu.pendingAction
                    menu.pendingAction = ""
                    shell.logoutOpen = false
                    shell.command("session-action", action)
                }
            }
        }
    }

    onVisibleChanged: if (!visible) pendingAction = ""
}
