import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components"
import "../../components"
import "../../style"
ColumnLayout {
    id: page
    required property var shell
    readonly property var state: shell.state.shellModules || ({})
    property bool dirty: false
    property bool saving: false
    property bool confirmTrust: false
    property bool confirmReset: false
    property int loadedRevision: -1
    spacing: 16
    function reloadEditor() { editor.text = JSON.stringify(state.document || {}, null, 2); dirty = false; loadedRevision = state.revision ?? 0 }
    onStateChanged: if (!dirty && loadedRevision !== (state.revision ?? 0)) reloadEditor()
    PageTitle { shell: page.shell; title: "Shell modules" }
    HelpText { shell: page.shell; message: "Edit built-in shell block layout and appearance. Zero width or height uses the built-in size; inherit follows the desktop palette. Saving applies immediately." }
    Text { text: page.state.path || ""; color: Theme.muted; wrapMode: Text.WrapAnywhere; Layout.fillWidth: true }
    Text { visible: page.dirty && page.loadedRevision !== page.state.revision; text: shell.tr("The file changed externally. Reload before saving to avoid replacing newer changes."); color: Theme.danger; wrapMode: Text.WordWrap; Layout.fillWidth: true }
    ScrollView {
        Layout.fillWidth: true; Layout.preferredHeight: 260; clip: true
        TextArea { id: editor; objectName: "moduleJsonEditor"; font.family: "monospace"; font.pixelSize: 12; color: Theme.text; wrapMode: TextEdit.NoWrap; selectByMouse: true; readOnly: page.saving; onTextChanged: page.dirty = true; background: Rectangle { color: Theme.surface; radius: 12 } }
    }
    RowLayout {
        ShellButton { text: shell.tr("Validate JSON"); onClicked: shell.command("module-validate", editor.text) }
        ShellButton { text: shell.tr("Save JSON"); active: true; enabled: page.dirty && !page.saving; onClicked: { page.saving = true; shell.command("module-save", editor.text) } }
        ShellButton { text: shell.tr("Reload editor"); onClicked: page.reloadEditor() }
    }
    HelpText { shell: page.shell; message: page.state.status || "" }
    HelpText { shell: page.shell; message: JSON.stringify(page.state.errors || {}) === "{}" ? "" : JSON.stringify(page.state.errors) }
    ShellButton { text: page.confirmReset ? shell.tr("Confirm restore built-in modules") : shell.tr("Restore built-in modules..."); onClicked: { if (page.confirmReset) { shell.command("module-reset", ""); page.dirty = false; page.loadedRevision = -1; page.confirmReset = false } else page.confirmReset = true } }
    Connections {
        target: page.shell
        function onCommandCompleted(method, result) {
            if (method !== "module-save" || !page.saving) return
            page.saving = false
            if (result.error) return
            editor.text = JSON.stringify(result.shellModules.document, null, 2)
            page.dirty = false; page.loadedRevision = result.shellModules.revision
        }
    }
    Component.onCompleted: reloadEditor()
}
