import QtQuick
import QtQuick.Layouts
import Quickshell
import Quickshell.Wayland
import "../components"
import "../configuration"
import "../style"
AnimatedPanel {
    id: wizard
    required property var shell
    property int step: 0
    implicitWidth: 640; implicitHeight: 540
    exclusionMode: ExclusionMode.Ignore
    WlrLayershell.layer: WlrLayer.Overlay
    WlrLayershell.namespace: "ludash-setup"
    WlrLayershell.keyboardFocus: WlrKeyboardFocus.Exclusive
    color: "transparent"
    Rectangle { anchors.fill: parent; radius: Theme.radius; color: Theme.background; border.color: Theme.accent }
    ColumnLayout {
        anchors.fill: parent; anchors.margins: 32; spacing: 20
        Text { text: "LuDash  /  " + (wizard.step + 1) + " · 4"; color: Theme.accent; font.family: Theme.font; font.pixelSize: 14 }
        Text { text: shell.tr(["Make yourself at home", "Connect your desktop", "Make it yours", "Ready when you are"][wizard.step]); color: Theme.text; font.pixelSize: 27; Layout.fillWidth: true; wrapMode: Text.WordWrap }
        ColumnLayout {
            visible: wizard.step === 0; spacing: 16
            Text { text: shell.tr("A quiet workspace, built around your windows. Choose a language to begin."); color: Theme.muted; wrapMode: Text.WordWrap; Layout.fillWidth: true }
            RowLayout {
                ShellButton { text: "English"; active: shell.state.language === "en_US"; onClicked: shell.command("language", "en_US") }
                ShellButton { text: shell.tr("Traditional Chinese"); active: shell.state.language === "zh_TW"; onClicked: shell.command("language", "zh_TW") }
            }
            Text { text: shell.tr("This is a development preview. Start in a nested session while evaluating it."); color: Theme.muted; wrapMode: Text.WordWrap; Layout.fillWidth: true }
        }
        ColumnLayout {
            visible: wizard.step === 1; spacing: 16
            Text { text: shell.tr((shell.state.network || {}).label || "Checking network"); color: Theme.accent; wrapMode: Text.WordWrap; Layout.fillWidth: true }
            Text { text: shell.tr("Existing system connections are reused automatically. You can continue offline and change your network later."); color: Theme.text; wrapMode: Text.WordWrap; Layout.fillWidth: true }
            ShellButton { text: shell.tr("Configure network"); onClicked: shell.configureNetwork() }
            Text { text: shell.tr("Connection settings open in NetworkManager's editor. Passwords stay in that editor. Close it to return here."); color: Theme.muted; wrapMode: Text.WordWrap; Layout.fillWidth: true }
        }
        ColumnLayout {
            visible: wizard.step === 2; spacing: 16
            AppearanceControls { shell: wizard.shell; Layout.fillWidth: true }
            RowLayout {
                ShellButton { text: shell.tr("Florist"); onClicked: shell.command("wallpaper-default", "") }
                ShellButton { text: shell.tr("Dusk"); onClicked: shell.command("wallpaper", 0) }
                ShellButton { text: shell.tr("Forest"); onClicked: shell.command("wallpaper", 1) }
            }
            Text { text: shell.tr("Changes are saved immediately. Choose your own wallpaper from Desktop settings."); color: Theme.muted; wrapMode: Text.WordWrap; Layout.fillWidth: true }
        }
        ColumnLayout {
            visible: wizard.step === 3; spacing: 16
            Text { text: shell.tr("Open apps from the diamond at the top left. Switch workspaces with Super + 1–4. Open a console with Super + Enter."); color: Theme.text; wrapMode: Text.WordWrap; Layout.fillWidth: true }
            Text { text: shell.tr("The gear opens settings, including this guide. Input method setup is available there too."); color: Theme.muted; wrapMode: Text.WordWrap; Layout.fillWidth: true }
        }
        Item { Layout.fillHeight: true }
        RowLayout {
            ShellButton { text: shell.tr("Back"); enabled: wizard.step > 0; onClicked: wizard.step-- }
            Item { Layout.fillWidth: true }
            ShellButton { text: shell.tr(wizard.step === 3 ? "Start desktop" : "Continue"); active: true; onClicked: { if (wizard.step < 3) wizard.step++; else shell.command("finish-setup", "") } }
        }
    }
}
