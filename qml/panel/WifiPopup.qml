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
    anchors.top: true
    anchors.right: true
    margins.top: Theme.barHeight + 8
    margins.right: 10
    implicitWidth: moduleWidth(390)
    implicitHeight: moduleHeight(420)
    exclusionMode: ExclusionMode.Ignore
    WlrLayershell.layer: WlrLayer.Overlay
    WlrLayershell.namespace: "lunadash-wifi-popup"
    color: "transparent"

    readonly property var network: shell.state.network || ({})
    readonly property var wifi: network.wifi || []
    readonly property var wifiDevice: (network.devices || []).find(device => String(device.type).toLowerCase() === "wifi") || ({})
    readonly property bool wifiEnabled: Boolean(wifiDevice.device) && wifiDevice.state !== "unavailable"
    property real reveal: opened ? 1 : 0
    Behavior on reveal { NumberAnimation { duration: Math.max(140, Theme.motion); easing.type: Easing.OutCubic } }

    function action(request) { shell.command("network", JSON.stringify(request)) }

    Rectangle {
        anchors.fill: parent
        radius: 22
        color: Theme.surfaceOpaque
        border.width: 1
        border.color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.38)
        opacity: popup.reveal
        scale: 0.94 + 0.06 * popup.reveal
        transformOrigin: Item.TopRight
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 18
        spacing: 10
        opacity: popup.reveal

        RowLayout {
            Layout.fillWidth: true
            LineIcon { width: 20; height: 20; name: "network"; ink: Theme.accent }
            Text { Layout.fillWidth: true; text: shell.tr("Wi-Fi"); color: Theme.text; font.family: Theme.font; font.pixelSize: 18; font.weight: Font.DemiBold }
            SoftSwitch {
                checked: popup.wifiEnabled
                onToggled: popup.action({action:"wifi-enable", enabled:checked})
            }
            ShellButton { text: "×"; Accessible.name: shell.tr("Close"); onClicked: shell.wifiPopupOpen = false }
        }

        Text {
            Layout.fillWidth: true
            text: popup.network.connected ? shell.tr("Connected") : shell.tr("Disconnected")
            color: popup.network.internet ? Theme.accent : Theme.muted
            font.family: Theme.font
            font.pixelSize: 11
        }

        SoftField {
            id: password
            Layout.fillWidth: true
            placeholderText: shell.tr("Wi-Fi password")
            echoMode: TextInput.Password
        }

        ListView {
            id: wifiList
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 6
            model: popup.wifi
            boundsBehavior: Flickable.StopAtBounds
            delegate: Rectangle {
                required property var modelData
                width: wifiList.width
                height: 54
                radius: 14
                color: modelData.active
                    ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.16)
                    : networkMouse.containsMouse ? Theme.controlHover : Theme.control
                border.width: modelData.active ? 1 : 0
                border.color: Theme.accent
                Behavior on color { ColorAnimation { duration: Theme.motion } }
                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 10
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 1
                        Text { Layout.fillWidth: true; text: modelData.ssid || shell.tr("Hidden network"); color: Theme.text; font.family: Theme.font; font.pixelSize: 13; elide: Text.ElideRight }
                        Text { text: (modelData.signal || 0) + "%  ·  " + (modelData.security || shell.tr("Open")); color: Theme.muted; font.family: Theme.font; font.pixelSize: 10 }
                    }
                    Text { text: modelData.active ? "✓" : "›"; color: modelData.active ? Theme.accent : Theme.muted; font.family: Theme.font; font.pixelSize: 18 }
                }
                MouseArea {
                    id: networkMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: if (!modelData.active) popup.action({action:"wifi-connect", ssid:modelData.ssid, password:password.text})
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            ShellButton { text: shell.tr("Scan"); onClicked: popup.action({action:"wifi-scan"}) }
            Item { Layout.fillWidth: true }
            ShellButton { text: shell.tr("More"); onClicked: { shell.wifiPopupOpen = false; shell.command("open-settings", "network") } }
        }
    }
}
