import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Quickshell
import "../../components"
import "../../style"

ColumnLayout {
    id: editor
    required property var shell
    required property string role
    required property string title
    property bool saving: false
    property var choices: []
    spacing: 10

    readonly property var stored: (shell.state.defaultApps || {})[role] || []
    readonly property string storedKey: JSON.stringify(stored)

    // Built once: the desktop-entry scan is asynchronous, and rebuilding the
    // model on every state poll would reset the selector to its first entry.
    function buildChoices() {
        const defaultLabel = editor.role === "terminal"
            ? editor.shell.tr("Konsole (default)")
            : editor.shell.tr("LunaDash default")
        const list = [{ label: defaultLabel, command: [] }]
        const seen = {}
        for (const entry of DesktopEntries.applications.values) {
            if (entry.noDisplay) continue
            const raw = entry.command || (entry.desktopEntry || {}).command
            const command = Array.isArray(raw) ? raw.slice() : (typeof raw === "string" && raw.length > 0 ? [raw] : [])
            if (command.length === 0) continue
            const key = command.join(" ")
            if (seen[key]) continue
            seen[key] = true
            list.push({ label: String(entry.name || entry.id), command: command })
        }
        editor.choices = list
        editor.syncSelector()
    }
    function describe(command) {
        return Array.isArray(command) ? command.join(" ") : String(command || "")
    }
    function indexForStored() {
        for (let i = 0; i < editor.choices.length; i++) {
            const choice = editor.choices[i]
            if (JSON.stringify(choice.command) === editor.storedKey)
                return i
        }
        return 0
    }
    function syncSelector() {
        const index = editor.indexForStored()
        selector.currentIndex = index
    }
    function apply(command) {
        editor.saving = true
        editor.shell.command("default-apps", JSON.stringify({ [editor.role]: command }))
    }

    Text { text: editor.shell.tr(editor.title); color: Theme.text; font.pixelSize: 16; font.family: Theme.font }
    Text {
        Layout.fillWidth: true
        text: editor.shell.tr("Choose an application to run for this action.")
        color: Theme.muted; font.pixelSize: 11; font.family: Theme.font; wrapMode: Text.WordWrap
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 10
        StyledComboBox {
            id: selector
            Layout.fillWidth: true
            model: editor.choices
            textRole: "label"
            enabled: !editor.saving
            onActivated: index => {
                const choice = editor.choices[index]
                if (choice) editor.apply(Array.from(choice.command))
            }
        }
        ShellButton { text: editor.shell.tr("Open"); onClicked: editor.shell.launch(editor.role) }
    }
    Text {
        Layout.fillWidth: true
        visible: true
        text: editor.stored.length > 0
            ? editor.shell.tr("Command: ") + editor.describe(editor.stored)
            : editor.shell.tr("Command: ") + editor.describe(editor.choices.length > 0 ? editor.choices[0].command : []) + editor.shell.tr(" (built-in)")
        color: Theme.muted; font.pixelSize: 11; font.family: Theme.font; elide: Text.ElideRight
    }

    Connections {
        target: editor.shell
        function onCommandCompleted(method, result) {
            if (method !== "default-apps" || !editor.saving) return
            editor.saving = false
            if (result.error) return
            editor.syncSelector()
        }
    }

    Component.onCompleted: {
        editor.buildChoices()
    }
    Timer {
        interval: 500
        repeat: true
        running: true
        onTriggered: {
            const installed = DesktopEntries.applications.values.filter(entry => !entry.noDisplay).length
            if (editor.choices.length !== installed + 1)
                editor.buildChoices()
        }
    }
}
