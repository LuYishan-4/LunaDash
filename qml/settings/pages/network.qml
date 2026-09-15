import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components"
import "../../components"
import "../../style"
import "../components" as SettingsComponents

ColumnLayout {
    id: page
    required property var shell

    readonly property var network: shell.state.network || ({})

    spacing: 16

    PageTitle { shell: page.shell; title: "Network" }

    SettingsComponents.SettingsCard {
        title: shell.tr("Network adapters")
        description: shell.tr("Detected wired and wireless interfaces.")
        Text {
            Layout.fillWidth: true
            visible: (page.network.devices || []).length === 0
            text: shell.tr("No network adapters were reported.")
            color: Theme.muted; font.family: Theme.font; font.pixelSize: 12
        }
        Repeater {
            model: page.network.devices || []
            ColumnLayout {
                required property var modelData
                Layout.fillWidth: true
                spacing: 2
                Text { Layout.fillWidth: true; text: modelData.device; color: Theme.text; font.family: Theme.font; font.pixelSize: 13; elide: Text.ElideRight }
                Text {
                    Layout.fillWidth: true
                    visible: text.length > 0
                    text: [modelData.type, modelData.state, modelData.connection].filter(Boolean).join("  ·  ")
                    color: Theme.muted; font.family: Theme.font; font.pixelSize: 11; elide: Text.ElideRight
                }
            }
        }
    }

    SettingsComponents.SettingsCard {
        title: shell.tr("Saved network configurations")
        description: shell.tr("Reconnect to profiles managed by NetworkManager.")
        Text {
            Layout.fillWidth: true
            visible: (page.network.connections || []).length === 0
            text: shell.tr("No saved connections were reported.")
            color: Theme.muted; font.family: Theme.font; font.pixelSize: 12
        }
        Repeater {
            model: page.network.connections || []
            RowLayout {
                required property var modelData
                Layout.fillWidth: true
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2
                    Text { Layout.fillWidth: true; text: modelData.name; color: Theme.text; font.family: Theme.font; font.pixelSize: 13; elide: Text.ElideRight }
                    Text { Layout.fillWidth: true; text: [modelData.type, modelData.state].filter(Boolean).join("  ·  "); color: Theme.muted; font.family: Theme.font; font.pixelSize: 11; elide: Text.ElideRight }
                }
                ShellButton {
                    text: shell.tr("Connect")
                    enabled: modelData.state !== "activated"
                    onClicked: shell.command("network", JSON.stringify({action: "connection-up", name: modelData.name}))
                }
            }
        }
    }

    SettingsComponents.SettingsCard {
        title: shell.tr("Wi-Fi")
        description: shell.tr("Search nearby wireless networks and connect through NetworkManager.")
        RowLayout {
            Layout.fillWidth: true
            SoftField {
                id: wifiPassword
                Layout.fillWidth: true
                placeholderText: shell.tr("Wi-Fi password")
                echoMode: TextInput.Password
            }
            ShellButton { text: shell.tr("Search Wi-Fi"); onClicked: shell.command("network", JSON.stringify({action: "wifi-scan"})) }
        }
        Text {
            Layout.fillWidth: true
            visible: (page.network.wifi || []).length === 0
            text: shell.tr("No wireless networks were reported. Search to scan again.")
            color: Theme.muted; font.family: Theme.font; font.pixelSize: 12
        }
        Repeater {
            model: page.network.wifi || []
            RowLayout {
                required property var modelData
                Layout.fillWidth: true
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2
                    Text { Layout.fillWidth: true; text: modelData.ssid; color: Theme.text; font.family: Theme.font; font.pixelSize: 13; elide: Text.ElideRight }
                    Text { Layout.fillWidth: true; text: [modelData.signal + "%", modelData.security].filter(Boolean).join("  ·  "); color: Theme.muted; font.family: Theme.font; font.pixelSize: 11; elide: Text.ElideRight }
                }
                ShellButton {
                    text: shell.tr("Connect")
                    enabled: !modelData.active
                    onClicked: shell.command("network", JSON.stringify({action: "wifi-connect", ssid: modelData.ssid, password: wifiPassword.text}))
                }
            }
        }
    }

    HelpText { shell: page.shell; message: "Wi-Fi credentials are managed by NetworkManager. Use the system network editor for secured networks." }
    HelpText { shell: page.shell; message: page.network.label || "Checking network" }
}
