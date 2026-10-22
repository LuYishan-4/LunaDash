import QtQuick
import QtQuick.Layouts
import "../components"
import "../../components"
import "../../style"
import "../components" as SettingsComponents

ColumnLayout {
    id: page
    required property var shell

    readonly property var devices: (shell.state.network || ({})).bluetooth || []

    spacing: 16

    PageTitle { shell: page.shell; title: "Bluetooth" }

    SettingsComponents.SettingsCard {
        title: shell.tr("Nearby devices")
        description: shell.tr("Search for Bluetooth devices with the system adapter.")
        ShellButton { text: shell.tr("Search for devices"); onClicked: shell.command("network", JSON.stringify({action: "bluetooth-scan"})) }
        Text { wrapMode: Text.Wrap; Layout.minimumWidth: 0;
            Layout.fillWidth: true
            visible: page.devices.length === 0
            text: shell.tr("No Bluetooth devices were reported. Search to scan again.")
            color: Theme.muted; font.family: Theme.font; font.pixelSize: 12
        }
        Repeater {
            model: page.devices
            ColumnLayout {
                required property var modelData
                Layout.fillWidth: true
                spacing: 2
                Text { Layout.minimumWidth: 0; Layout.fillWidth: true; text: modelData.name || modelData.address; color: Theme.text; font.family: Theme.font; font.pixelSize: 13; elide: Text.ElideRight }
                Text { Layout.minimumWidth: 0; Layout.fillWidth: true; visible: Boolean(modelData.name); text: modelData.address; color: Theme.muted; font.family: Theme.font; font.pixelSize: 11; elide: Text.ElideRight }
            }
        }
    }

    HelpText { shell: page.shell; message: "Bluetooth discovery uses bluetoothctl. Pairing and connection actions remain available through the system Bluetooth manager." }
    ToolList { shell: page.shell; category: "bluetooth" }
}
