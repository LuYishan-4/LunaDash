import QtQuick
import QtQuick.Layouts
import "../../components"
import "../../style"

ColumnLayout {
    id: form
    required property var shell
    required property string targetId
    readonly property var target: ((shell.state.settingsApi || {}).targets || [])
        .find(item => item.id === targetId) || ({})
    property var draft: ({})
    property string loadedRevision: ""
    property bool dirty: false
    property bool saving: false
    property string pendingTarget: ""
    property string message: ""
    readonly property bool stale: dirty && loadedRevision !== String(target.revision || "")
    spacing: 10

    function reload() {
        draft = JSON.parse(JSON.stringify(target.values || {}))
        loadedRevision = String(target.revision || "")
        dirty = false
        message = ""
    }
    onTargetChanged: if (!dirty && !saving) reload()
    onTargetIdChanged: reload()
    Component.onCompleted: reload()

    SchemaOptions {
        Layout.fillWidth: true
        shell: form.shell
        schema: form.target.schema || ({})
        values: form.draft
        enabled: !form.saving
        onEdited: (key, value) => {
            const next = Object.assign({}, form.draft)
            next[key] = value
            form.draft = next
            form.dirty = true
        }
    }
    HelpText {
        visible: form.stale
        shell: form.shell
        message: "Settings changed. Reload before applying this edit."
    }
    RowLayout {
        visible: Object.keys(form.target.schema || {}).length > 0
        Layout.fillWidth: true
        ShellButton {
            text: form.shell.tr("Apply settings")
            active: true
            enabled: form.dirty && !form.saving && !form.stale
            onClicked: {
                form.pendingTarget = form.targetId
                form.saving = true
                form.shell.command("settings-update", JSON.stringify({
                    target: form.targetId, revision: form.loadedRevision, changes: form.draft
                }))
            }
        }
        ShellButton {
            text: form.shell.tr("Reload")
            enabled: !form.saving
            onClicked: form.reload()
        }
        ShellButton {
            text: form.shell.tr("Defaults")
            enabled: !form.saving
            onClicked: {
                const next = Object.assign({}, form.draft)
                const schema = form.target.schema || {}
                Object.keys(schema).forEach(key => {
                    if (!schema[key].readOnly) next[key] = schema[key].default
                })
                form.draft = next
                form.dirty = true
            }
        }
    }
    Text {
        visible: form.message.length > 0
        Layout.fillWidth: true
        Layout.minimumWidth: 0
        text: form.message
        wrapMode: Text.Wrap
        color: Theme.danger
        font.family: Theme.font
        font.pixelSize: 12
    }
    Connections {
        target: form.shell
        function onCommandCompleted(method, result) {
            if (method !== "settings-update" || !form.saving || result.settingsTarget !== form.pendingTarget)
                return
            form.saving = false
            const relevant = form.pendingTarget === form.targetId
            form.pendingTarget = ""
            if (!relevant) { form.reload(); return }
            if (result.error) {
                form.message = form.shell.tr(result.error)
            } else {
                form.dirty = false
                form.reload()
            }
        }
    }
}
