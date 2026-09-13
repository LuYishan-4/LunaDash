import QtQuick
import QtQuick.Layouts
import "../components"
ColumnLayout {
    id: page
    required property var shell
    spacing: 22
    PageTitle { shell: page.shell; title: "Printers and storage" }
    HelpText { shell: page.shell; message: "Manage printers and storage with the installed system tools. Review destructive operations in those tools before confirming." }
    ToolList { shell: page.shell; category: "devices" }
}
