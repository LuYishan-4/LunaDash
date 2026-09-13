import QtQuick
import QtQuick.Layouts
import "../components"
ColumnLayout {
    id: page
    required property var shell
    spacing: 22
    PageTitle { shell: page.shell; title: "Bluetooth" }
    HelpText { shell: page.shell; message: "Pair devices, connect accessories and manage adapters in the system Bluetooth tool." }
    ToolList { shell: page.shell; category: "bluetooth" }
}
