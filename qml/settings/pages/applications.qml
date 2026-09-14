import QtQuick
import QtQuick.Layouts
import "../components"
import "../../components"
ColumnLayout {
    id: page
    required property var shell
    spacing: 20
    PageTitle { shell: page.shell; title: "Applications and startup" }
    DefaultAppEditor { shell: page.shell; role: "terminal"; title: "Default terminal"; Layout.fillWidth: true }
    DefaultAppEditor { shell: page.shell; role: "files"; title: "Default file manager"; Layout.fillWidth: true }
    HelpText { shell: page.shell; message: "Choose an installed application for each action, or Custom command… to enter an argument array such as [\"kitty\", \"fish\"] or [\"dolphin\"]. LunaDash default uses the built-in Files or Kitty with the LunaDash Fish profile. Commands run with your user permissions; shell operators are not expanded." }
    HelpText { shell: page.shell; message: "The default terminal requires Kitty and Fish. Your Kitty settings, login shell, Fish configuration and system MIME defaults remain under your control." }
    RowLayout {
        ShellButton { text: shell.tr("Package manager"); onClicked: shell.launch("packages") }
        ShellButton { text: shell.tr("Manage metadata plugins"); onClicked: shell.launch("plugins") }
        ShellButton { text: "X11"; onClicked: { shell.settingsOpen = false; shell.x11Open = true } }
    }
    HelpText { shell: page.shell; message: "Start these built-in apps with the next LunaDash session" }
    Flow {
        Layout.fillWidth: true; spacing: 8
        Repeater {
            model: ["files", "console", "monitor", "welcome"]
            ShellButton {
                required property string modelData
                text: modelData; active: ((shell.state.appearance || {}).startupApps || []).includes(modelData)
                onClicked: { const apps = ((shell.state.appearance || {}).startupApps || []).slice(); const index = apps.indexOf(modelData); if (index >= 0) apps.splice(index, 1); else apps.push(modelData); shell.setAppearance({startupApps: apps}) }
            }
        }
    }
    HelpText { shell: page.shell; message: "Startup selection currently covers LunaDash tools. General desktop-entry autostart and session restoration are not implemented." }
    ToolList { shell: page.shell; category: "applications" }
}
