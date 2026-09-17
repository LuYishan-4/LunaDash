import "../modules"
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Quickshell
import Quickshell.Wayland
import "../components"
import "../configuration"
import "../style"
ModuleSurface {
    id: wizard
    moduleId: "setup"
    property int step: 0
    readonly property var languages: [
        {code:"en_US", name:"English"},
        {code:"zh_TW", name:"Traditional Chinese"},
        {code:"zh_CN", name:"Simplified Chinese"},
        {code:"ja_JP", name:"Japanese"}
    ]
    implicitWidth: moduleWidth(640); implicitHeight: moduleHeight(540)
    exclusionMode: ExclusionMode.Ignore
    WlrLayershell.layer: WlrLayer.Overlay
    WlrLayershell.namespace: "lunadash-setup"
    WlrLayershell.keyboardFocus: opened ? WlrKeyboardFocus.Exclusive : WlrKeyboardFocus.None
    color: "transparent"
    Rectangle { anchors.fill: parent; radius: moduleRadius; color: moduleBackground; border.color: moduleAccent }
    ColumnLayout {
        anchors.fill: parent; anchors.margins: 32; spacing: 20
        RowLayout {
            Layout.fillWidth: true; spacing: 12
            LunaDashLogo { width: 42; height: 42; animated: Theme.animations }
            Text { text: "LunaDash  /  " + (wizard.step + 1) + " · 4"; color: moduleAccent; font.family: Theme.font; font.pixelSize: 14 }
            Item { Layout.fillWidth: true }
        }
        Text { text: shell.tr(["Make yourself at home", "Connect your desktop", "Make it yours", "Ready when you are"][wizard.step]); color: moduleForeground; font.pixelSize: 27; Layout.fillWidth: true; wrapMode: Text.WordWrap }
        Flickable {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentWidth: width
            contentHeight: stepContent.implicitHeight
            boundsBehavior: Flickable.StopAtBounds
            ColumnLayout {
                id: stepContent
                width: parent.width
                spacing: 16
                ColumnLayout {
                    visible: wizard.step === 0; spacing: 16
                    Text { text: shell.tr("A quiet workspace, built around your windows. Choose a language to begin."); color: Theme.muted; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                    RowLayout {
                        Layout.fillWidth: true
                        Text { text: shell.tr("Interface language"); color: moduleForeground; Layout.fillWidth: true }
                        StyledComboBox {
                            translationContext: wizard.shell
                            model: wizard.languages
                            textRole: "name"
                            valueRole: "code"
                            currentIndex: Math.max(0, wizard.languages.findIndex(item => item.code === shell.state.language))
                            onActivated: shell.setLanguage(currentValue)
                        }
                    }
                    Text { text: shell.tr("This is a development preview. Start in a nested session while evaluating it."); color: Theme.muted; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                }
                ColumnLayout {
                    visible: wizard.step === 1; spacing: 16
                    Text { text: shell.tr((shell.state.network || {}).label || "Checking network"); color: moduleAccent; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                    Text { text: shell.tr("Existing system connections are reused automatically. You can continue offline and change your network later."); color: moduleForeground; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                    ShellButton { text: shell.tr("Configure network"); onClicked: shell.configureNetwork() }
                    Text { text: shell.tr("Connection settings open in NetworkManager's editor. Passwords stay in its editor. Close it to return here."); color: Theme.muted; wrapMode: Text.WordWrap; Layout.fillWidth: true }
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
                    Text { text: shell.tr("Open apps from the diamond at the top left. Switch workspaces with Super + 1–4. Open a terminal with Super + Enter."); color: moduleForeground; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                    Text { text: shell.tr("The gear opens settings, including this guide. Input method setup is available there too."); color: Theme.muted; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                }
            }
        }
        RowLayout {
            ShellButton { text: shell.tr("Back"); enabled: wizard.step > 0; onClicked: wizard.step-- }
            Item { Layout.fillWidth: true }
            ShellButton { text: shell.tr(wizard.step === 3 ? "Start desktop" : "Continue"); active: true; onClicked: { if (wizard.step < 3) wizard.step++; else shell.command("finish-setup", "") } }
        }
    }
}
