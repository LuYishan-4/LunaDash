import QtQuick
import Quickshell
import Quickshell.Io
import "wallpaper"
import "panel"
import "overview"
import "launcher"
import "settings"
import "session"
import "feedback"
import "setup"
import "style"
import "compatibility"

ShellRoot {
    id: root
    property var state: ({ workspace: 0, clients: [], language: "en_US", wallpaper: 0 })
    property string bin: Quickshell.env("LUDASH_BIN_DIR")
    property bool launcherOpen: false
    property bool settingsOpen: false
    property bool x11Open: false
    readonly property bool overviewOpen: (state.appearance || {}).overview ?? false
    function setAppearance(changes) { command("appearance", JSON.stringify(changes)) }
    onStateChanged: {
        Theme.accent = (state.appearance || {}).accent || "#9ccbfb"; Theme.barHeight = (state.appearance || {}).panelHeight || 40
        Theme.animations = (state.appearance || {}).animations ?? true; Theme.animationDuration = (state.appearance || {}).animationDuration ?? 220
        if (setupPaused) {
            const mapped = state.clients.some(client => client.mapped)
            if (mapped) setupEditorMapped = true
            if ((!mapped && setupEditorMapped) || (!setupEditorMapped && ++setupWaitTicks >= 15)) setupPaused = false
        }
    }
    property bool logoutOpen: false
    property bool setupPaused: false
    property bool setupEditorMapped: false
    property int setupWaitTicks: 0
    function configureNetwork() {
        command("configure-network", "")
        if (state.setupComplete === false) { setupPaused = true; setupEditorMapped = false; setupWaitTicks = 0 }
    }
    property string errorMessage: ""
    property string focusedTitle: (state.clients.find(client => client.focused) || {}).title || "LuDash"
    function tr(source) { return (state.translations || {})[source] || source }
    function command(method, value) {
        action.queue.push([method, String(value ?? "")]); dispatch()
    }
    function dispatch() {
        if (action.running || action.queue.length === 0) return
        action.command = [bin + "/ludashctl"].concat(action.queue.shift()); action.running = true
    }
    Process {
        id: action
        property var queue: []
        stdout: StdioCollector { onStreamFinished: { try { const result = JSON.parse(text); if (result.error) root.errorMessage = result.error } catch (error) { root.errorMessage = "Could not contact the desktop." } } }
        onExited: (exitCode, exitStatus) => { if (exitCode !== 0 && !root.errorMessage) root.errorMessage = "Could not contact the desktop."; Qt.callLater(root.dispatch) }
    }
    function launch(id) { Quickshell.execDetached([bin + "/ludash-desktop", "--app", id]); launcherOpen = false }
    Process {
        id: status
        command: [root.bin + "/ludashctl", "status"]
        stdout: StdioCollector { onStreamFinished: { try { root.state = JSON.parse(text); if (root.state.shutdown) Qt.quit() } catch (error) { console.warn(error) } } }
    }
    Timer { interval: 700; running: true; repeat: true; triggeredOnStart: true; onTriggered: if (!status.running) status.running = true }
    Wallpaper { shell: root }
    TopPanel { shell: root }
    Overview { shell: root; opened: root.overviewOpen }
    Launcher { shell: root; opened: root.launcherOpen }
    SettingsPanel { shell: root; opened: root.settingsOpen }
    SetupWizard { shell: root; opened: root.state.setupComplete === false && !root.setupPaused }
    LogoutPanel { shell: root; opened: root.logoutOpen }
    X11Launcher { shell: root; opened: root.x11Open }
    Message { shell: root; opened: root.errorMessage.length > 0 }
}
