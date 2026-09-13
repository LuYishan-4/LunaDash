import QtQuick
import Quickshell
import Quickshell.Io
import "wallpaper"
import "panel"
import "dock"
import "launcher"
import "settings"

ShellRoot {
    id: root
    property var state: ({ workspace: 0, clients: [], language: "zh_TW", wallpaper: 0 })
    property string bin: Quickshell.env("LUDASH_BIN_DIR")
    property bool launcherOpen: false
    property bool settingsOpen: false
    function tr(source) { return (state.translations || {})[source] || source }
    function command(method, value) { Quickshell.execDetached([bin + "/ludashctl", method, String(value ?? "")]) }
    function launch(id) { Quickshell.execDetached([bin + "/ludash-desktop", "--app", id]); launcherOpen = false }
    Process {
        id: status
        command: [root.bin + "/ludashctl", "status"]
        stdout: StdioCollector { onStreamFinished: { try { root.state = JSON.parse(text); if (root.state.shutdown) Qt.quit() } catch (error) { console.warn(error) } } }
    }
    Timer { interval: 700; running: true; repeat: true; triggeredOnStart: true; onTriggered: if (!status.running) status.running = true }
    Wallpaper { shell: root }
    TopPanel { shell: root }
    Dock { shell: root }
    Launcher { shell: root; visible: root.launcherOpen }
    SettingsPanel { shell: root; visible: root.settingsOpen }
}
