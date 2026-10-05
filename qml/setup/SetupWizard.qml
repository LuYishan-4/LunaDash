import "../modules"
import "../welcome"
import Quickshell
import Quickshell.Wayland

ModuleSurface {
    id: wizard
    moduleId: "setup"
    implicitWidth: moduleWidth(800)
    implicitHeight: moduleHeight(660)
    exclusionMode: ExclusionMode.Ignore
    WlrLayershell.layer: WlrLayer.Overlay
    WlrLayershell.namespace: "lunadash-setup"
    WlrLayershell.keyboardFocus: opened ? WlrKeyboardFocus.Exclusive : WlrKeyboardFocus.None
    color: "transparent"

    WelcomeContent {
        anchors.fill: parent
        shell: wizard.shell
        onFinished: wizard.shell.command("finish-setup", "")
        onSettingsRequested: page => {
            wizard.shell.command("finish-setup", "")
            wizard.shell.command("open-settings", page)
        }
    }
}
