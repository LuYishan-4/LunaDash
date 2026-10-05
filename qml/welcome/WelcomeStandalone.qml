import QtQuick
import "../style"

WelcomeContent {
    shell: adapter
    onFinished: welcomeBridge.finish()
    onSettingsRequested: page => welcomeBridge.command("open-settings", page)
    QtObject {
        id: adapter
        readonly property var state: welcomeBridge.state
        function tr(text) { return (state.translations || {})[text] || text }
        function command(method, value) { welcomeBridge.command(method, value) }
    }
    Binding { target: Theme; property: "palette"; value: adapter.state.palette || ({}) }
    Binding { target: Theme; property: "animations"; value: (adapter.state.appearance || {}).animations ?? true }
    Binding { target: Theme; property: "font"; value: (adapter.state.appearance || {}).fontFamily || "sans-serif" }
}
