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
    implicitHeight: moduleHeight(pendingAction === "" ? 292 : 188)
    Behavior on implicitHeight { NumberAnimation { duration: Theme.motion; easing.type: Easing.OutCubic } }
    exclusionMode: ExclusionMode.Ignore
    WlrLayershell.layer: WlrLayer.Overlay
    WlrLayershell.namespace: "lunadash-logout"
    color: "transparent"

    Rectangle { anchors.fill: parent; radius: moduleRadius; color: moduleBackground; border.color: Theme.border }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 18
        spacing: 9

        Text {
            Layout.fillWidth: true
            text: menu.pendingAction === "logout" ? shell.tr("Log out of LunaDash?")
                  : menu.pendingAction === "reboot" ? shell.tr("Restart this computer?")
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
            ShellButton { Layout.fillWidth: true; iconName: "power"; text: shell.tr("Log out"); onClicked: menu.pendingAction = "logout" }
            ShellButton { Layout.fillWidth: true; iconName: "moon"; text: shell.tr("Suspend"); enabled: menu.availability.suspend === true; onClicked: { shell.logoutOpen = false; shell.command("session-action", "suspend") } }
            ShellButton { Layout.fillWidth: true; iconName: "update"; text: shell.tr("Restart"); enabled: menu.availability.reboot === true; onClicked: menu.pendingAction = "reboot" }
            ShellButton { Layout.fillWidth: true; iconName: "power"; destructive: true; text: shell.tr("Shut down"); enabled: menu.availability.poweroff === true; onClicked: menu.pendingAction = "poweroff" }
            ShellButton { Layout.fillWidth: true; quiet: true; text: shell.tr("Cancel"); onClicked: shell.logoutOpen = false }
        }

        Text {
            visible: menu.pendingAction !== ""
            Layout.fillWidth: true
            text: shell.tr("Open applications may contain unsaved work.")
            color: Theme.muted
            font.family: Theme.font
            font.pixelSize: 11
            wrapMode: Text.WordWrap
        }

        RowLayout {
            visible: menu.pendingAction !== ""
            Layout.fillWidth: true
            ShellButton { Layout.fillWidth: true; quiet: true; text: shell.tr("Cancel"); onClicked: menu.pendingAction = "" }
            ShellButton {
                Layout.fillWidth: true
                text: menu.pendingAction === "logout" ? shell.tr("Confirm log out")
                    : menu.pendingAction === "reboot" ? shell.tr("Confirm restart")
                    : shell.tr("Confirm shut down")
                active: true
                iconName: menu.pendingAction === "reboot" ? "update" : "power"
                destructive: menu.pendingAction === "poweroff"
                onClicked: {
                    const action = menu.pendingAction
                    menu.pendingAction = ""
                    shell.logoutOpen = false
                    if (action === "logout")
                        shell.command("quit", "confirm")
                    else
                        shell.command("session-action", action)
                }
            }
        }
    }

    onVisibleChanged: if (!visible) pendingAction = ""
}
