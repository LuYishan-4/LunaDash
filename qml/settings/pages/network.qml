import QtQuick
import QtQuick.Layouts
import "../components"
ColumnLayout {
    id: page
    required property var shell
    spacing: 22
    PageTitle { shell: page.shell; title: "Network" }
    HelpText { shell: page.shell; message: "Existing connections are reused. Configure Wi-Fi, Ethernet, VPN and connection profiles in NetworkManager; passwords stay in its editor." }
    HelpText { shell: page.shell; message: (shell.state.network || {}).label || "Checking network" }
    ToolList { shell: page.shell; category: "network" }
}
