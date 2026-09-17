import QtQuick
import QtQuick.Layouts
import Quickshell.Io
import "../components"
import "../components" as SettingsComponents
import "../../components"
import "../../style"

ColumnLayout {
    id: page
    required property var shell
    spacing: 16

    readonly property var network: shell.state.network || ({})
    readonly property var connections: network.connections || []
    readonly property var proxy: shell.state.proxy || ({enabled:false, http:"", https:"", socks:"", bypass:""})
    property string section: "connections"
    property string selectedConnection: connections.length > 0 ? connections[0].name : ""
    property string diagnosticsText: ""

    function networkAction(request) {
        shell.command("network", JSON.stringify(request))
    }

    function selectedProfile() {
        return connections.find(connection => connection.name === selectedConnection) || ({})
    }

    PageTitle { shell: page.shell; title: "Internet and network" }

    RowLayout {
        Layout.fillWidth: true
        spacing: 8
        ShellButton { text: shell.tr("Connections"); active: page.section === "connections"; onClicked: page.section = "connections" }
        ShellButton { text: shell.tr("IP and DNS"); active: page.section === "ip"; onClicked: page.section = "ip" }
        ShellButton { text: shell.tr("Internet Options"); active: page.section === "internet"; onClicked: page.section = "internet" }
        ShellButton { text: shell.tr("Diagnostics"); active: page.section === "diagnostics"; onClicked: { page.section = "diagnostics"; diagnostics.running = true } }
    }

    SettingsComponents.SettingsCard {
        visible: page.section === "connections"
        title: shell.tr("Network adapters")
        description: shell.tr("Enable, disable, connect and disconnect wired or wireless adapters managed by NetworkManager.")

        RowLayout {
            Layout.fillWidth: true
            ShellButton { text: shell.tr("Enable networking"); onClicked: page.networkAction({action:"networking-enable", enabled:true}) }
            ShellButton { text: shell.tr("Disable networking"); onClicked: page.networkAction({action:"networking-enable", enabled:false}) }
            ShellButton { text: shell.tr("Enable Wi-Fi"); onClicked: page.networkAction({action:"wifi-enable", enabled:true}) }
            ShellButton { text: shell.tr("Disable Wi-Fi"); onClicked: page.networkAction({action:"wifi-enable", enabled:false}) }
            Item { Layout.fillWidth: true }
        }

        Text {
            Layout.fillWidth: true
            visible: (page.network.devices || []).length === 0
            text: shell.tr("No network adapters were reported.")
            color: Theme.muted
            font.family: Theme.font
        }

        Repeater {
            model: page.network.devices || []
            Rectangle {
                required property var modelData
                Layout.fillWidth: true
                implicitHeight: adapterRow.implicitHeight + 18
                radius: 12
                color: Theme.control
                border.width: 1
                border.color: Theme.border
                RowLayout {
                    id: adapterRow
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 9
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2
                        Text { Layout.fillWidth: true; text: modelData.device; color: Theme.text; font.family: Theme.font; font.pixelSize: 13 }
                        Text { Layout.fillWidth: true; text: [modelData.type, modelData.state, modelData.connection].filter(Boolean).join("  ·  "); color: Theme.muted; font.family: Theme.font; font.pixelSize: 11; elide: Text.ElideRight }
                    }
                    ShellButton {
                        text: modelData.state === "connected" ? shell.tr("Disconnect") : shell.tr("Connect")
                        onClicked: page.networkAction({action: modelData.state === "connected" ? "device-disconnect" : "device-connect", device:modelData.device})
                    }
                }
            }
        }
    }

    SettingsComponents.SettingsCard {
        visible: page.section === "connections"
        title: shell.tr("Saved network configurations")
        description: shell.tr("Reconnect, disconnect, enable automatic connection, or forget NetworkManager profiles including Ethernet, Wi-Fi and VPN.")
        Repeater {
            model: page.connections
            RowLayout {
                required property var modelData
                Layout.fillWidth: true
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2
                    Text { Layout.fillWidth: true; text: modelData.name; color: Theme.text; font.family: Theme.font; font.pixelSize: 13; elide: Text.ElideRight }
                    Text { Layout.fillWidth: true; text: [modelData.type, modelData.device, modelData.state, modelData.uuid].filter(Boolean).join("  ·  "); color: Theme.muted; font.family: Theme.font; font.pixelSize: 11; elide: Text.ElideMiddle }
                }
                ShellButton {
                    text: modelData.autoconnect ? shell.tr("Auto") : shell.tr("Manual")
                    active: modelData.autoconnect
                    onClicked: page.networkAction({action:"connection-autoconnect", name:modelData.name, enabled:!modelData.autoconnect})
                }
                ShellButton {
                    text: modelData.state === "activated" ? shell.tr("Disconnect") : shell.tr("Connect")
                    onClicked: page.networkAction({action:modelData.state === "activated" ? "connection-down" : "connection-up", name:modelData.name})
                }
                ShellButton { text: shell.tr("Forget"); onClicked: page.networkAction({action:"connection-delete", name:modelData.name}) }
            }
        }
    }

    SettingsComponents.SettingsCard {
        visible: page.section === "connections"
        title: shell.tr("Wi-Fi")
        description: shell.tr("Search nearby wireless networks and connect through NetworkManager.")
        RowLayout {
            Layout.fillWidth: true
            SoftField { id: wifiPassword; Layout.fillWidth: true; placeholderText: shell.tr("Wi-Fi password"); echoMode: TextInput.Password }
            ShellButton { text: shell.tr("Search Wi-Fi"); onClicked: page.networkAction({action:"wifi-scan"}) }
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
                    Text { Layout.fillWidth: true; text: [modelData.signal + "%", modelData.security, modelData.device].filter(Boolean).join("  ·  "); color: Theme.muted; font.family: Theme.font; font.pixelSize: 11 }
                }
                ShellButton { text: shell.tr("Connect"); enabled: !modelData.active; onClicked: page.networkAction({action:"wifi-connect", ssid:modelData.ssid, password:wifiPassword.text}) }
            }
        }
    }

    SettingsComponents.SettingsCard {
        visible: page.section === "ip"
        title: shell.tr("IPv4, IPv6 and DNS")
        description: shell.tr("Choose a saved connection and configure DHCP or a manual IPv4 address, gateway, DNS servers, IPv6 policy and route metric.")

        RowLayout {
            Layout.fillWidth: true
            Text { text: shell.tr("Connection"); color: Theme.text; font.family: Theme.font }
            StyledComboBox {
                id: profileBox
                Layout.fillWidth: true
                model: page.connections.map(connection => connection.name)
                currentIndex: Math.max(0, model.indexOf(page.selectedConnection))
                onActivated: page.selectedConnection = currentText
            }
        }
        SoftField { id: ipv4Address; Layout.fillWidth: true; placeholderText: shell.tr("IPv4 address / prefix, for example 192.168.1.25/24") }
        SoftField { id: ipv4Gateway; Layout.fillWidth: true; placeholderText: shell.tr("IPv4 gateway") }
        SoftField { id: dnsServers; Layout.fillWidth: true; placeholderText: shell.tr("DNS servers separated by spaces") }
        SoftField { id: routeMetric; Layout.fillWidth: true; placeholderText: shell.tr("Route metric (-1 for automatic)"); text: "-1" }
        RowLayout {
            Layout.fillWidth: true
            ShellButton { text: shell.tr("Use DHCP"); enabled: page.selectedConnection.length > 0; onClicked: page.networkAction({action:"connection-ipv4-auto", name:page.selectedConnection}) }
            ShellButton {
                text: shell.tr("Apply manual IPv4")
                enabled: page.selectedConnection.length > 0 && ipv4Address.text.length > 0
                onClicked: page.networkAction({action:"connection-ipv4-manual", name:page.selectedConnection, address:ipv4Address.text, gateway:ipv4Gateway.text, dns:dnsServers.text})
            }
            ShellButton { text: shell.tr("IPv6 automatic"); enabled: page.selectedConnection.length > 0; onClicked: page.networkAction({action:"connection-ipv6-auto", name:page.selectedConnection}) }
            ShellButton { text: shell.tr("Disable IPv6"); enabled: page.selectedConnection.length > 0; onClicked: page.networkAction({action:"connection-ipv6-disabled", name:page.selectedConnection}) }
        }
        RowLayout {
            Layout.fillWidth: true
            ShellButton {
                text: shell.tr("Apply route metric")
                enabled: page.selectedConnection.length > 0
                onClicked: page.networkAction({action:"connection-route-metric", name:page.selectedConnection, metric:Number(routeMetric.text)})
            }
            ShellButton { text: shell.tr("Reconnect to apply"); enabled: page.selectedConnection.length > 0; onClicked: { page.networkAction({action:"connection-down", name:page.selectedConnection}); page.networkAction({action:"connection-up", name:page.selectedConnection}) } }
            Item { Layout.fillWidth: true }
        }
    }

    SettingsComponents.SettingsCard {
        visible: page.section === "internet"
        title: shell.tr("General")
        description: shell.tr("Default browser, file associations and browsing-data controls belong to the selected browser. LunaDash keeps the system default application entry here for convenience.")
        ShellButton { text: shell.tr("Default applications and associations"); onClicked: shell.command("open-settings", "applications") }
    }

    SettingsComponents.SettingsCard {
        visible: page.section === "internet"
        title: shell.tr("Security")
        description: shell.tr("Manage firewall policy and system TLS trust. LunaDash does not expose obsolete SSL/TLS version switches that would weaken every application.")
        RowLayout {
            Layout.fillWidth: true
            ShellButton { text: shell.tr("Firewall rules and zones"); onClicked: shell.command("system-tool", "firewall") }
            ShellButton { text: shell.tr("Certificates and keys"); onClicked: shell.command("system-tool", "certificates") }
            Item { Layout.fillWidth: true }
        }
    }

    SettingsComponents.SettingsCard {
        visible: page.section === "internet"
        title: shell.tr("Privacy")
        description: shell.tr("Control application permissions, accessibility, screen locking and privacy-related desktop behavior.")
        ShellButton { text: shell.tr("Open privacy settings"); onClicked: shell.command("open-settings", "privacy") }
    }

    SettingsComponents.SettingsCard {
        visible: page.section === "internet"
        title: shell.tr("Content")
        description: shell.tr("Certificate stores, personal keys and trust are managed through the system key and certificate tool.")
        ShellButton { text: shell.tr("Manage certificates"); onClicked: shell.command("system-tool", "certificates") }
    }

    SettingsComponents.SettingsCard {
        visible: page.section === "internet"
        title: shell.tr("Connections and proxy")
        description: shell.tr("Set a session-wide proxy for applications launched by LunaDash. Leave fields empty when that protocol should connect directly.")
        ShellButton {
            text: page.proxy.enabled ? shell.tr("Proxy enabled") : shell.tr("Proxy disabled")
            active: page.proxy.enabled
            onClicked: shell.command("proxy", JSON.stringify({enabled:!page.proxy.enabled, http:proxyHttp.text, https:proxyHttps.text, socks:proxySocks.text, bypass:proxyBypass.text}))
        }
        SoftField { id: proxyHttp; Layout.fillWidth: true; placeholderText: shell.tr("HTTP proxy, for example http://127.0.0.1:8080"); text: page.proxy.http || "" }
        SoftField { id: proxyHttps; Layout.fillWidth: true; placeholderText: shell.tr("HTTPS proxy"); text: page.proxy.https || "" }
        SoftField { id: proxySocks; Layout.fillWidth: true; placeholderText: shell.tr("SOCKS proxy"); text: page.proxy.socks || "" }
        SoftField { id: proxyBypass; Layout.fillWidth: true; placeholderText: shell.tr("Proxy bypass list, comma separated"); text: page.proxy.bypass || "" }
        RowLayout {
            Layout.fillWidth: true
            ShellButton { text: shell.tr("Save proxy settings"); onClicked: shell.command("proxy", JSON.stringify({enabled:page.proxy.enabled, http:proxyHttp.text, https:proxyHttps.text, socks:proxySocks.text, bypass:proxyBypass.text})) }
            ShellButton { text: shell.tr("Advanced connection editor"); onClicked: shell.command("system-tool", "network") }
            Item { Layout.fillWidth: true }
        }
    }

    SettingsComponents.SettingsCard {
        visible: page.section === "internet"
        title: shell.tr("Programs")
        description: shell.tr("Choose default applications and file associations used when links, files or terminal actions are opened.")
        ShellButton { text: shell.tr("Default programs"); onClicked: shell.command("open-settings", "applications") }
    }

    SettingsComponents.SettingsCard {
        visible: page.section === "internet"
        title: shell.tr("Advanced")
        description: shell.tr("Reset NetworkManager connectivity, flush the resolver cache, inspect routes and DNS, or open the full connection editor.")
        RowLayout {
            Layout.fillWidth: true
            ShellButton { text: shell.tr("Flush DNS cache"); onClicked: flushDns.running = true }
            ShellButton { text: shell.tr("Reset networking"); onClicked: { page.networkAction({action:"networking-enable", enabled:false}); page.networkAction({action:"networking-enable", enabled:true}) } }
            ShellButton { text: shell.tr("Show diagnostics"); onClicked: { page.section = "diagnostics"; diagnostics.running = true } }
            Item { Layout.fillWidth: true }
        }
    }

    SettingsComponents.SettingsCard {
        visible: page.section === "diagnostics"
        title: shell.tr("Network diagnostics")
        description: shell.tr("Current addresses, routes, DNS resolver status and NetworkManager summary.")
        RowLayout {
            Layout.fillWidth: true
            ShellButton { text: shell.tr("Refresh diagnostics"); onClicked: diagnostics.running = true }
            ShellButton { text: shell.tr("Flush DNS cache"); onClicked: flushDns.running = true }
            Item { Layout.fillWidth: true }
        }
        Text {
            Layout.fillWidth: true
            text: page.diagnosticsText || shell.tr("Collecting network diagnostics…")
            color: Theme.muted
            font.family: Theme.font
            font.pixelSize: 11
            wrapMode: Text.WrapAnywhere
        }
    }

    HelpText { shell: page.shell; message: page.network.label || "Checking network" }

    Process {
        id: diagnostics
        command: ["sh", "-c", "printf '=== IP ===\\n'; ip -brief address 2>&1; printf '\\n=== ROUTES ===\\n'; ip route 2>&1; printf '\\n=== DNS ===\\n'; (resolvectl status 2>&1 || true); printf '\\n=== NETWORKMANAGER ===\\n'; (nmcli general 2>&1 || true)"]
        stdout: StdioCollector { onStreamFinished: page.diagnosticsText = text.trim() }
        stderr: StdioCollector { onStreamFinished: if (text.trim()) page.diagnosticsText += "\n" + text.trim() }
    }

    Process {
        id: flushDns
        command: ["resolvectl", "flush-caches"]
        onExited: (code, status) => { if (page.section === "diagnostics") diagnostics.running = true }
    }
}
