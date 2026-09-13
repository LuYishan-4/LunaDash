import QtQuick
import QtQuick.Layouts
import "../../components"
import "../../style"
ColumnLayout {
    id: list
    required property var shell
    required property string category
    spacing: 18; Layout.fillWidth: true
    Repeater {
        model: (shell.state.settingsTools || []).filter(tool => tool.category === list.category)
        ColumnLayout {
            required property var modelData
            Layout.fillWidth: true; spacing: 8
            Text { text: shell.tr(modelData.name); color: Theme.text; font.family: Theme.font; font.pixelSize: 16; wrapMode: Text.WordWrap; Layout.fillWidth: true }
            HelpText { shell: list.shell; message: modelData.available ? (modelData.host ? "Changes apply to the desktop hosting this LuDash session." : "Opens the installed system settings tool. Changes are managed by that tool.") : modelData.host ? "A compatible host desktop and the optional settings package are required." : "Install the optional package below to configure this feature." }
            Text { visible: !modelData.available; text: modelData.package; color: Theme.accent; wrapMode: Text.WrapAnywhere; Layout.fillWidth: true }
            ShellButton { text: shell.tr("Open settings tool"); enabled: modelData.available; onClicked: shell.command("system-tool", modelData.id) }
        }
    }
}
