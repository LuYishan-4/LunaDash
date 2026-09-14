import QtQuick
import QtQuick.Layouts
import "../components"
ColumnLayout {
    id: page
    required property var shell
    spacing: 22
    PageTitle { shell: page.shell; title: "Users, date and time" }
    HelpText { shell: page.shell; message: "Account and clock changes may require system authorization. LunaDash opens the installed system tool without storing passwords." }
    ToolList { shell: page.shell; category: "system" }
}
