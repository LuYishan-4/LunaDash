import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Quickshell
import Quickshell.Wayland
import "../components"
import "../style"
AnimatedPanel {
    id: panel
    required property var shell
    implicitWidth: 520; implicitHeight: 270
    exclusionMode: ExclusionMode.Ignore
    WlrLayershell.layer: WlrLayer.Overlay
    WlrLayershell.namespace: "ludash-x11-launcher"
    WlrLayershell.keyboardFocus: WlrKeyboardFocus.Exclusive
    color: "transparent"
    Rectangle { anchors.fill: parent; color: Theme.background; radius: Theme.radius }
    ColumnLayout {
        anchors.fill: parent; anchors.margins: 28; spacing: 15
        RowLayout {
            Text { text: shell.tr("Run an X11 application"); color: Theme.text; font.pixelSize: 22; Layout.fillWidth: true }
            ShellButton { text: "×"; onClicked: shell.x11Open = false }
        }
        Text { text: (shell.state.xwayland || {}).available ? shell.tr("X11 apps open inside a compatibility window") : ((shell.state.xwayland || {}).error || shell.tr("XWayland is unavailable")); color: Theme.muted; wrapMode: Text.WordWrap; Layout.fillWidth: true }
        TextField {
            id: command; Layout.fillWidth: true; placeholderText: "application --argument"; color: Theme.text
            background: Rectangle { color: Theme.surface; radius: 12; border.color: command.activeFocus ? Theme.accent : Theme.border }
            onAccepted: run.clicked(); focus: true
        }
        Text { text: shell.tr("Enter a program and arguments. Shell operators are not evaluated."); color: Theme.muted; font.pixelSize: 11; wrapMode: Text.WordWrap; Layout.fillWidth: true }
        ShellButton { id: run; text: shell.tr("Launch"); active: true; enabled: command.text.trim().length > 0 && Boolean((shell.state.xwayland || {}).available); onClicked: { shell.command("launch-x11", command.text); shell.x11Open = false } }
    }
}
