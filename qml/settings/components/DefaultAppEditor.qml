import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../components"
import "../../style"
ColumnLayout {
    id: editor
    required property var shell
    required property string role
    required property string title
    property bool dirty: false
    property bool saving: false
    spacing: 10
    Text { text: editor.shell.tr(editor.title); color: Theme.text; font.pixelSize: 16 }
    TextField {
        id: command; readOnly: editor.saving; Layout.fillWidth: true; font.family: "monospace"; color: Theme.text
        placeholderText: editor.role === "terminal" ? '["kitty", "fish"]' : '["dolphin"]'
        background: Rectangle { color: Theme.surface; radius: 10; border.color: Theme.border }
        onTextEdited: editor.dirty = true
    }
    RowLayout {
        ShellButton { text: shell.tr("Save command"); enabled: editor.dirty && !editor.saving; onClicked: { try { const value = JSON.parse(command.text); editor.saving = true; shell.command("default-apps", JSON.stringify({[editor.role]: value})) } catch (error) { shell.errorMessage = "Enter a JSON argument array." } } }
        ShellButton { text: shell.tr("Use LunaDah default"); onClicked: { command.text = "[]"; shell.command("default-apps", JSON.stringify({[editor.role]: []})); editor.dirty = false } }
        ShellButton { text: shell.tr("Open"); onClicked: shell.launch(editor.role) }
    }
    Connections {
        target: editor.shell
        function onCommandCompleted(method, result) {
            if (method !== "default-apps" || !editor.saving) return
            editor.saving = false
            if (!result.error) { command.text = JSON.stringify(result.defaultApps[editor.role]); editor.dirty = false }
        }
    }
    Component.onCompleted: command.text = JSON.stringify((shell.state.defaultApps || {})[role] || [])
}
