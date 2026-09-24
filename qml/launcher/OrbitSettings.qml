import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import "../components"
import "../settings/components"
import "../style"

SettingsCard {
    id: settings
    required property var shell
    title: shell.tr("Orbit launcher")
    description: shell.tr("Edit apps, folders, links and search engines. Commands are argument arrays; search text is URL encoded.")
    Text {
        Layout.fillWidth: true
        text: (shell.state.orbit || {}).path || ""
        color: Theme.muted
        font.family: Theme.font
        elide: Text.ElideMiddle
    }
    ScrollView {
        Layout.fillWidth: true
        Layout.preferredHeight: 260
        clip: true
        SoftTextArea {
            id: editor
            width: parent.width
            text: JSON.stringify(((shell.state.orbit || {}).document || {}), null, 2)
            wrapMode: TextEdit.Wrap
            characterLimit: 32768
            Accessible.name: shell.tr("Orbit configuration")
        }
    }
    RowLayout {
        Layout.fillWidth: true
        ShellButton {
            text: shell.tr("Restore defaults")
            onClicked: shell.command("orbit-reset", "")
        }
        Item {
            Layout.fillWidth: true
        }
        ShellButton {
            text: shell.tr("Reload")
            onClicked: editor.text = JSON.stringify(((shell.state.orbit || {}).document || {}), null, 2)
        }
        ShellButton {
            text: shell.tr("Save")
            active: true
            onClicked: shell.command("orbit-save", editor.text)
        }
    }
    Text {
        Layout.fillWidth: true
        visible: Boolean((shell.state.orbit || {}).error)
        text: (shell.state.orbit || {}).error || ""
        color: Theme.warning
        font.family: Theme.font
        wrapMode: Text.Wrap
    }
}
