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
    property int knownApplicationCount: -1
    spacing: 10

    readonly property var stored: (shell.state.defaultApps || {})[role] || []
    readonly property string storedKey: JSON.stringify(stored)

    // Store source labels, not their translations. The selector translates only
    // its presentation, so changing language never rebuilds an open model.
    function buildChoices() {
        const applications = DesktopEntries.applications.values
        const defaultLabel = editor.role === "terminal"
            ? "Konsole (default)"
            : editor.role === "browser"
                ? "Google Chrome (default)"
                : "LunaDash default"
        const entries = []
        const seen = {}
        for (const entry of applications) {
            const command = entry.command
            if (!command || command.length === 0) continue
            const argv = Array.from(command)
            const key = argv.join(" ")
            if (seen[key]) continue
            seen[key] = true
            entries.push({ label: String(entry.name || entry.id), command: argv })
        }
        entries.sort((left, right) => left.label.localeCompare(right.label))
        editor.knownApplicationCount = applications.length
        editor.choices = [{ label: defaultLabel, command: [] }].concat(entries)
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
        selector.currentIndex = editor.indexForStored()
    }
    function apply(command) {
        editor.saving = true
        editor.shell.command("default-apps", JSON.stringify({ [editor.role]: command }))
    }

    Text { text: editor.shell.tr(editor.title); color: Theme.text; font.pixelSize: 16; font.family: Theme.font }
    Text { Layout.minimumWidth: 0;
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
            translationContext: editor.shell
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
    Text { Layout.minimumWidth: 0;
        Layout.fillWidth: true
        visible: true
        text: editor.stored.length > 0
            ? editor.shell.tr("Command: ") + editor.describe(editor.stored)
            : editor.shell.tr("Using the built-in default.")
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

    Component.onCompleted: editor.buildChoices()
    Timer {
        interval: 500
        repeat: true
        running: true
        onTriggered: {
            if (editor.knownApplicationCount !== DesktopEntries.applications.values.length)
                editor.buildChoices()
        }
    }
}
