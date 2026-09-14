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
    property bool dirty: false
    property bool saving: false
    property bool custom: false
    property var choices: []
    property int customIndex: 0
    spacing: 10

    readonly property var stored: (shell.state.defaultApps || {})[role] || []
    readonly property string storedKey: JSON.stringify(stored)

    // Built once: the desktop-entry scan is asynchronous, and rebuilding the
    // model on every state poll would reset the selector to its first entry.
    function buildChoices() {
        const list = [{ label: editor.shell.tr("LunaDah default"), command: [], custom: false }]
        const seen = {}
        for (const entry of DesktopEntries.applications.values) {
            if (entry.noDisplay) continue
            const raw = entry.command
            const command = Array.isArray(raw) ? raw.slice() : (typeof raw === "string" && raw.length > 0 ? [raw] : [])
            if (command.length === 0) continue
            const key = command.join(" ")
            if (seen[key]) continue
            seen[key] = true
            list.push({ label: String(entry.name || entry.id), command: command, custom: false })
        }
        list.push({ label: editor.shell.tr("Custom command…"), command: [], custom: true })
        editor.choices = list
        editor.customIndex = list.length - 1
    }
    function describe(command) {
        return Array.isArray(command) ? command.join(" ") : String(command || "")
    }
    function indexForStored() {
        for (let i = 0; i < editor.choices.length; i++) {
            const choice = editor.choices[i]
            if (!choice.custom && JSON.stringify(choice.command) === editor.storedKey)
                return i
        }
        return editor.customIndex
    }
    function syncSelector() {
        const index = editor.indexForStored()
        selector.currentIndex = index
        editor.custom = index === editor.customIndex
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
                if (choice.custom) {
                    editor.custom = true
                } else {
                    editor.custom = false
                    editor.apply(Array.from(choice.command))
                }
            }
        }
        ShellButton { text: editor.shell.tr("Open"); onClicked: editor.shell.launch(editor.role) }
    }

    Text {
        Layout.fillWidth: true
        visible: !editor.custom
        text: editor.stored.length > 0
            ? editor.shell.tr("Command: ") + editor.describe(editor.stored)
            : editor.shell.tr("Command: ") + editor.describe(editor.choices.length > 0 ? editor.choices[0].command : []) + editor.shell.tr(" (built-in)")
        color: Theme.muted; font.pixelSize: 11; font.family: Theme.font; elide: Text.ElideRight
    }

    TextField {
        id: command
        visible: editor.custom
        readOnly: editor.saving
        Layout.fillWidth: true
        Layout.preferredHeight: 38
        color: Theme.text
        font.family: "monospace"
        font.pixelSize: 12
        placeholderText: editor.role === "terminal" ? '["kitty", "fish"]' : '["dolphin"]'
        background: Rectangle { color: Theme.surface; radius: 10; border.color: Theme.border }
        onTextEdited: editor.dirty = true
    }
    RowLayout {
        visible: editor.custom
        ShellButton {
            text: editor.shell.tr("Save command")
            enabled: editor.dirty && !editor.saving
            onClicked: {
                try {
                    const value = JSON.parse(command.text)
                    if (!Array.isArray(value) || value.some(item => typeof item !== "string"))
                        throw new Error("not a string array")
                    editor.dirty = false
                    editor.apply(value)
                } catch (error) {
                    editor.shell.errorMessage = editor.shell.tr("Enter a JSON array of command arguments.")
                }
            }
        }
        ShellButton {
            text: editor.shell.tr("Use LunaDah default")
            onClicked: { editor.dirty = false; editor.custom = false; editor.apply([]) }
        }
    }

    Connections {
        target: editor.shell
        function onCommandCompleted(method, result) {
            if (method !== "default-apps" || !editor.saving) return
            editor.saving = false
            if (result.error) return
            const value = (result.defaultApps || {})[editor.role] || []
            command.text = JSON.stringify(value)
            editor.dirty = false
            editor.syncSelector()
        }
    }

    Component.onCompleted: {
        editor.buildChoices()
        command.text = JSON.stringify(editor.stored)
        editor.syncSelector()
    }
}
