import "../modules"
import QtQuick
import QtQuick.Layouts
import Quickshell
import Quickshell.Wayland
import "../components"
import "../style"

ModuleSurface {
    id: wizard
    moduleId: "setup"
    implicitWidth: moduleWidth(560)
    implicitHeight: moduleHeight(340)
    exclusionMode: ExclusionMode.Ignore
    WlrLayershell.layer: WlrLayer.Overlay
    WlrLayershell.namespace: "lunadash-setup"
    WlrLayershell.keyboardFocus: opened ? WlrKeyboardFocus.Exclusive : WlrKeyboardFocus.None
    color: "transparent"

    Rectangle { anchors.fill: parent; radius: moduleRadius; color: moduleBackground; border.color: moduleAccent }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 28
        spacing: 16

        RowLayout {
            Layout.fillWidth: true
            spacing: 14
            LunaDashLogo { Layout.preferredWidth: 42; Layout.preferredHeight: 42; animated: Theme.animations }
            Text {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                text: shell.tr("Welcome")
                color: moduleForeground
                font.family: Theme.font
                font.pixelSize: 27
                wrapMode: Text.WordWrap
            }
        }
        Text {
            Layout.fillWidth: true
            text: shell.tr("If you need help, please visit:")
            color: moduleForeground
            font.family: Theme.font
            font.pixelSize: moduleFontSize
            wrapMode: Text.WordWrap
        }
        ShellButton {
            Layout.fillWidth: true
            text: "luyishan-4.github.io/LunaDash/"
            quiet: true
            onClicked: {
                shell.command("finish-setup", "")
                shell.openUrl("https://luyishan-4.github.io/LunaDash/")
            }
        }
        Item { Layout.fillHeight: true }
        ShellButton {
            Layout.alignment: Qt.AlignRight
            Layout.maximumWidth: parent.width
            text: shell.tr("Start desktop")
            active: true
            focus: true
            onClicked: shell.command("finish-setup", "")
        }
    }
}
