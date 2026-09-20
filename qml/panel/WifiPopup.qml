import "../modules"
import QtQuick
import QtQuick.Layouts
import Quickshell
import Quickshell.Wayland
import "../components"
import "../style"

ModuleSurface {
    id: popup
    moduleId: "overview"
    extensionTarget: "network"
    anchors.top: true
    anchors.right: true
    margins.top: Theme.barHeight + 8
    margins.right: 10
    implicitWidth: moduleWidth(410)
    implicitHeight: moduleHeight(500)
    exclusionMode: ExclusionMode.Ignore
    WlrLayershell.layer: WlrLayer.Overlay
    WlrLayershell.namespace: "lunadash-wifi-popup"
    WlrLayershell.keyboardFocus: opened && passwordPage ? WlrKeyboardFocus.Exclusive : WlrKeyboardFocus.None
    color: "transparent"

    readonly property var network: shell.state.network || ({})
    readonly property var wifi: network.wifi || []
    readonly property var wifiDevice: (network.devices || []).find(device => {
        const type = String(device.type || "").toLowerCase()
        return type === "wifi" || type === "802-11-wireless" || type === "wireless"
    }) || ({})
    readonly property bool hasWifi: network.hasWifi ?? Boolean(wifiDevice.device)
    readonly property bool wifiEnabled: network.wifiEnabled ?? (popup.hasWifi && wifiDevice.state !== "unavailable")
    readonly property bool ethernetConnected: Boolean(network.ethernetConnected)
    readonly property bool wifiConnected: Boolean(network.wifiConnected)
    readonly property string activeConnection: String(network.primaryConnection || "")

    property var selectedNetwork: null
    property bool passwordPage: false
    property bool scanning: false
    property real reveal: opened ? 1 : 0

    Behavior on reveal {
        NumberAnimation { duration: Math.max(140, Theme.motion); easing.type: Easing.OutCubic }
    }

    function action(request) {
        shell.command("network", JSON.stringify(request))
    }

    function networkNeedsPassword(networkEntry) {
        if (networkEntry.secured !== undefined)
            return Boolean(networkEntry.secured)
        const security = String(networkEntry.security || "").trim().toLowerCase()
        return security.length > 0 && security !== "--" && security !== "open" && security !== "none"
    }

    function scan() {
        if (!hasWifi || !wifiEnabled)
            return
        scanning = true
        action({action:"wifi-scan"})
        scanTimer.restart()
    }

    function chooseNetwork(entry) {
        if (!entry || entry.active)
            return
        selectedNetwork = entry
        password.text = ""
        if (networkNeedsPassword(entry)) {
            passwordPage = true
            Qt.callLater(function() {
                if (!popup.opened || !popup.passwordPage)
                    return
                password.forceActiveFocus(Qt.OtherFocusReason)
                password.prepareInputMethod()
            })
        } else {
            action({action:"wifi-connect", ssid:String(entry.ssid || ""), password:""})
        }
    }

    function closePasswordPage() {
        passwordPage = false
        selectedNetwork = null
        password.text = ""
    }

    onOpenedChanged: {
        if (!opened) {
            closePasswordPage()
            scanning = false
            return
        }
        closePasswordPage()
        if (hasWifi && wifiEnabled)
            Qt.callLater(scan)
    }

    Timer {
        id: scanTimer
        interval: 1400
        repeat: false
        onTriggered: popup.scanning = false
    }

    Rectangle {
        anchors.fill: parent
        radius: 24
        color: Theme.surfaceOpaque
        border.width: 1
        border.color: Qt.rgba(Theme.starlight.r, Theme.starlight.g, Theme.starlight.b, 0.30)
        opacity: popup.reveal
        scale: 0.95 + 0.05 * popup.reveal
        transformOrigin: Item.TopRight

        Rectangle {
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 18
            width: 72
            height: 72
            radius: 36
            color: Qt.rgba(Theme.moon.r, Theme.moon.g, Theme.moon.b, 0.035)
            border.width: 1
            border.color: Qt.rgba(Theme.moon.r, Theme.moon.g, Theme.moon.b, 0.14)
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 18
        spacing: 12
        opacity: popup.reveal

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            Rectangle {
                width: 36
                height: 36
                radius: 18
                color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.13)
                LineIcon {
                    anchors.centerIn: parent
                    width: 19
                    height: 19
                    name: "network"
                    ink: popup.network.connected ? Theme.moon : Theme.muted
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 1
                Text {
                    text: popup.passwordPage ? shell.tr("Join Wi-Fi network") : shell.tr("Network")
                    color: Theme.text
                    font.family: Theme.font
                    font.pixelSize: 18
                    font.weight: Font.DemiBold
                }
                Text {
                    Layout.fillWidth: true
                    text: popup.ethernetConnected
                        ? shell.tr("Ethernet connected")
                        : popup.wifiConnected
                            ? (popup.activeConnection.length ? popup.activeConnection : shell.tr("Wi-Fi connected"))
                            : shell.tr("Offline")
                    color: popup.network.connected ? Theme.accent : Theme.muted
                    font.family: Theme.font
                    font.pixelSize: 10
                    elide: Text.ElideRight
                }
            }

            ShellButton {
                visible: popup.passwordPage
                text: "‹"
                Accessible.name: shell.tr("Back")
                onClicked: popup.closePasswordPage()
            }

            ShellButton {
                text: "×"
                Accessible.name: shell.tr("Close")
                onClicked: shell.wifiPopupOpen = false
            }
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: Qt.rgba(Theme.starlight.r, Theme.starlight.g, Theme.starlight.b, 0.12)
        }

        ColumnLayout {
            visible: !popup.passwordPage
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 10

            Rectangle {
                visible: popup.ethernetConnected
                Layout.fillWidth: true
                Layout.preferredHeight: 60
                radius: 16
                color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.10)
                border.width: 1
                border.color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.24)

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 12
                    LineIcon { width: 20; height: 20; name: "network"; ink: Theme.accent }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 1
                        Text { text: shell.tr("Ethernet"); color: Theme.text; font.family: Theme.font; font.pixelSize: 13; font.weight: Font.DemiBold }
                        Text { text: popup.activeConnection.length ? popup.activeConnection : shell.tr("Wired connection active"); color: Theme.muted; font.family: Theme.font; font.pixelSize: 10; elide: Text.ElideRight; Layout.fillWidth: true }
                    }
                    Text { text: "●"; color: Theme.accent; font.pixelSize: 10 }
                }
            }

            Rectangle {
                visible: !popup.hasWifi
                Layout.fillWidth: true
                Layout.fillHeight: true
                radius: 18
                color: Qt.rgba(Theme.surface.r, Theme.surface.g, Theme.surface.b, 0.72)
                border.width: 1
                border.color: Theme.border

                ColumnLayout {
                    anchors.centerIn: parent
                    width: Math.min(parent.width - 40, 260)
                    spacing: 10
                    LineIcon { Layout.alignment: Qt.AlignHCenter; width: 46; height: 46; name: "network"; ink: Theme.muted }
                    Text {
                        Layout.fillWidth: true
                        text: shell.tr("No wireless adapter")
                        color: Theme.text
                        font.family: Theme.font
                        font.pixelSize: 17
                        font.weight: Font.DemiBold
                        horizontalAlignment: Text.AlignHCenter
                    }
                    Text {
                        Layout.fillWidth: true
                        text: shell.tr("This device does not currently expose a Wi-Fi network adapter.")
                        color: Theme.muted
                        font.family: Theme.font
                        font.pixelSize: 11
                        wrapMode: Text.WordWrap
                        horizontalAlignment: Text.AlignHCenter
                    }
                }
            }

            Rectangle {
                visible: popup.hasWifi && !popup.wifiEnabled
                Layout.fillWidth: true
                Layout.fillHeight: true
                radius: 18
                color: Qt.rgba(Theme.surface.r, Theme.surface.g, Theme.surface.b, 0.72)
                border.width: 1
                border.color: Theme.border

                ColumnLayout {
                    anchors.centerIn: parent
                    spacing: 12
                    LineIcon { Layout.alignment: Qt.AlignHCenter; width: 42; height: 42; name: "network"; ink: Theme.muted }
                    Text { Layout.alignment: Qt.AlignHCenter; text: shell.tr("Wi-Fi is off"); color: Theme.text; font.family: Theme.font; font.pixelSize: 17; font.weight: Font.DemiBold }
                    ShellButton {
                        Layout.alignment: Qt.AlignHCenter
                        active: true
                        text: shell.tr("Turn on Wi-Fi")
                        onClicked: popup.action({action:"wifi-enable", enabled:true})
                    }
                }
            }

            ColumnLayout {
                visible: popup.hasWifi && popup.wifiEnabled
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 8

                RowLayout {
                    Layout.fillWidth: true
                    Text {
                        Layout.fillWidth: true
                        text: popup.scanning ? shell.tr("Scanning for networks…") : shell.tr("Available networks")
                        color: Theme.muted
                        font.family: Theme.font
                        font.pixelSize: 11
                    }
                    ShellButton {
                        text: popup.scanning ? "…" : "↻"
                        enabled: !popup.scanning
                        Accessible.name: shell.tr("Scan")
                        onClicked: popup.scan()
                    }
                    SoftSwitch {
                        checked: popup.wifiEnabled
                        onToggled: popup.action({action:"wifi-enable", enabled:checked})
                    }
                }

                ListView {
                    id: wifiList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    spacing: 7
                    model: popup.wifi
                    boundsBehavior: Flickable.StopAtBounds

                    delegate: Rectangle {
                        required property var modelData
                        width: wifiList.width
                        height: 62
                        radius: 16
                        color: modelData.active
                            ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.14)
                            : networkMouse.containsMouse
                                ? Theme.controlHover
                                : Theme.control
                        border.width: modelData.active ? 1 : 0
                        border.color: Qt.rgba(Theme.moon.r, Theme.moon.g, Theme.moon.b, 0.42)
                        Behavior on color { ColorAnimation { duration: Theme.motion } }

                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 11
                            spacing: 10

                            Rectangle {
                                width: 36
                                height: 36
                                radius: 18
                                color: Qt.rgba(Theme.starlight.r, Theme.starlight.g, Theme.starlight.b, 0.07)
                                LineIcon { anchors.centerIn: parent; width: 18; height: 18; name: "network"; ink: modelData.active ? Theme.accent : Theme.text }
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2
                                Text {
                                    Layout.fillWidth: true
                                    text: modelData.ssid || shell.tr("Hidden network")
                                    color: Theme.text
                                    font.family: Theme.font
                                    font.pixelSize: 13
                                    font.weight: modelData.active ? Font.DemiBold : Font.Normal
                                    elide: Text.ElideRight
                                }
                                Text {
                                    text: (modelData.signal || 0) + "%  ·  " + (popup.networkNeedsPassword(modelData) ? shell.tr("Secured") : shell.tr("Open"))
                                    color: Theme.muted
                                    font.family: Theme.font
                                    font.pixelSize: 10
                                }
                            }

                            Text {
                                text: modelData.active ? "✓" : "›"
                                color: modelData.active ? Theme.accent : Theme.muted
                                font.family: Theme.font
                                font.pixelSize: 18
                            }
                        }

                        MouseArea {
                            id: networkMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: modelData.active ? Qt.ArrowCursor : Qt.PointingHandCursor
                            onClicked: popup.chooseNetwork(modelData)
                        }
                    }

                    Text {
                        anchors.centerIn: parent
                        visible: wifiList.count === 0 && !popup.scanning
                        text: shell.tr("No Wi-Fi networks found")
                        color: Theme.muted
                        font.family: Theme.font
                        font.pixelSize: 12
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                ShellButton {
                    text: shell.tr("Network settings")
                    onClicked: {
                        shell.wifiPopupOpen = false
                        shell.command("open-settings", "network")
                    }
                }
            }
        }

        ColumnLayout {
            visible: popup.passwordPage
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 14

            Item { Layout.fillHeight: true }

            Rectangle {
                Layout.alignment: Qt.AlignHCenter
                width: 62
                height: 62
                radius: 31
                color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.12)
                LineIcon { anchors.centerIn: parent; width: 28; height: 28; name: "network"; ink: Theme.moon }
            }

            Text {
                Layout.alignment: Qt.AlignHCenter
                text: popup.selectedNetwork ? String(popup.selectedNetwork.ssid || shell.tr("Wi-Fi network")) : shell.tr("Wi-Fi network")
                color: Theme.text
                font.family: Theme.font
                font.pixelSize: 20
                font.weight: Font.DemiBold
            }

            Text {
                Layout.alignment: Qt.AlignHCenter
                text: shell.tr("Enter the password for this network")
                color: Theme.muted
                font.family: Theme.font
                font.pixelSize: 11
            }

            SoftField {
                id: password
                Layout.fillWidth: true
                placeholderText: shell.tr("Password")
                echoMode: TextInput.Password
                Keys.onReturnPressed: {
                    if (text.length > 0 && popup.selectedNetwork) {
                        popup.action({action:"wifi-connect", ssid:String(popup.selectedNetwork.ssid || ""), password:text})
                        popup.closePasswordPage()
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                ShellButton { text: shell.tr("Cancel"); onClicked: popup.closePasswordPage() }
                Item { Layout.fillWidth: true }
                ShellButton {
                    text: shell.tr("Connect")
                    active: true
                    enabled: password.text.length > 0 && popup.selectedNetwork !== null
                    onClicked: {
                        popup.action({action:"wifi-connect", ssid:String(popup.selectedNetwork.ssid || ""), password:password.text})
                        popup.closePasswordPage()
                    }
                }
            }

            Item { Layout.fillHeight: true }
        }
    }
}
