import QtQuick
import QtQuick.Layouts
import "../components"
import "../../components"
ColumnLayout {
    id: page
    required property var shell
    spacing: 20
    PageTitle { shell: page.shell; title: "Privacy and accessibility" }
    ShellButton { text: shell.tr("Show user and host"); active: (shell.state.appearance || {}).showHostDetails ?? false; onClicked: shell.setAppearance({showHostDetails: !((shell.state.appearance || {}).showHostDetails ?? false)}) }
    ShellButton { text: shell.tr("Reduced motion"); active: !((shell.state.appearance || {}).animations ?? true); onClicked: shell.setAppearance({animations: !((shell.state.appearance || {}).animations ?? true)}) }
    ShellButton { text: shell.tr("Manage metadata plugins"); onClicked: shell.launch("plugins") }
    HelpText { shell: page.shell; message: "Native plugins are disabled by default. Enable only plugins you trust; metadata is not a sandbox." }
    HelpText { shell: page.shell; message: "LunaDah does not provide a secure lock screen, a notification service, screen-reader integration or portal permission management yet. Use the host desktop for these features while testing nested sessions." }
    ToolList { shell: page.shell; category: "privacy" }
}
