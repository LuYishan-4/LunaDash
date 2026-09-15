import QtQuick
import QtQuick.Layouts
import "../components"
import "../../components"
import "../../style"
ColumnLayout {
    id: page
    required property var shell
    spacing: 16
    PageTitle { shell: page.shell; title: "Power and battery" }
    Text { text: (shell.state.system || {}).batteryPercent >= 0 ? shell.tr("Battery") + "  " + shell.state.system.batteryPercent + "%" : shell.tr("No battery reported"); color: Theme.text; font.pixelSize: 18 }
    HelpText { shell: page.shell; message: "Power profile" }
    RowLayout {
        Repeater {
            model: (shell.state.power || {}).profiles || []
            ShellButton { required property string modelData; text: modelData; active: (shell.state.power || {}).current === modelData; enabled: !(shell.state.power || {}).busy; onClicked: shell.command("power-profile", modelData) }
        }
    }
    HelpText { shell: page.shell; visible: !(shell.state.power || {}).available; message: "Power profiles are unavailable. Install power-profiles-daemon and use a supported system service." }
    HelpText { shell: page.shell; message: (shell.state.power || {}).error || "Profile changes apply to the whole computer and are authorized by the system service." }
    HelpText { shell: page.shell; message: "Idle suspend, lid actions, backlight control and automatic screen locking are not managed by LunaDash yet. In nested sessions, keep using your host desktop's power settings." }
}
