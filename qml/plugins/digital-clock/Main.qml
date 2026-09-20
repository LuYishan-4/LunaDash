import QtQuick
import QtQuick.Layouts
import Quickshell
import Quickshell.Wayland

Item {
    id: plugin
    required property var shell
    required property var settings
    required property var context
    width: 0
    height: 0

    readonly property var appearance: shell ? (shell.state.appearance || {}) : ({})
    readonly property color accent: appearance.accent || "#9ccbfb"
    readonly property color primary: Qt.lighter(accent, 1.06)
    readonly property color secondary: Qt.lighter(accent, 1.42)
    readonly property string uiFont: appearance.fontFamily || Qt.application.font.family
    property date now: new Date()

    Timer {
        interval: 1000
        running: true
        repeat: true
        triggeredOnStart: true
        onTriggered: plugin.now = new Date()
    }

    PanelWindow {
        anchors.right: true
        anchors.bottom: true
        margins.right: 34
        margins.bottom: 34
        implicitWidth: 405
        implicitHeight: 112
        exclusiveZone: 0
        color: "transparent"
        WlrLayershell.layer: WlrLayer.Bottom
        WlrLayershell.namespace: "lunadash-plugin-digital-clock"
        WlrLayershell.keyboardFocus: WlrKeyboardFocus.None

        RowLayout {
            anchors.fill: parent
            spacing: 17

            RowLayout {
                Layout.alignment: Qt.AlignVCenter
                spacing: 3

                Text {
                    text: Qt.formatDateTime(plugin.now, "HH")
                    color: plugin.primary
                    style: Text.Outline
                    styleColor: Qt.rgba(0, 0, 0, 0.38)
                    font.family: plugin.uiFont
                    font.pixelSize: 62
                    font.weight: Font.Bold
                }
                Text {
                    text: ":"
                    color: plugin.accent
                    opacity: 0.92
                    style: Text.Outline
                    styleColor: Qt.rgba(0, 0, 0, 0.30)
                    font.family: plugin.uiFont
                    font.pixelSize: 56
                    font.weight: Font.DemiBold
                    Layout.alignment: Qt.AlignVCenter
                    Layout.topMargin: -5
                }
                Text {
                    text: Qt.formatDateTime(plugin.now, "mm")
                    color: plugin.primary
                    style: Text.Outline
                    styleColor: Qt.rgba(0, 0, 0, 0.38)
                    font.family: plugin.uiFont
                    font.pixelSize: 62
                    font.weight: Font.Bold
                }
            }

            Rectangle {
                Layout.preferredWidth: 2
                Layout.preferredHeight: 72
                Layout.alignment: Qt.AlignVCenter
                radius: 1
                color: Qt.rgba(0.92, 0.42, 0.48, 0.78)
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignVCenter
                spacing: -1

                Text {
                    text: Qt.formatDateTime(plugin.now, "MMMM").toUpperCase()
                    color: plugin.secondary
                    style: Text.Outline
                    styleColor: Qt.rgba(0, 0, 0, 0.30)
                    font.family: plugin.uiFont
                    font.pixelSize: 14
                    font.weight: Font.Bold
                    font.letterSpacing: 3.2
                }
                Text {
                    text: Qt.formatDateTime(plugin.now, "dd")
                    color: plugin.primary
                    style: Text.Outline
                    styleColor: Qt.rgba(0, 0, 0, 0.32)
                    font.family: plugin.uiFont
                    font.pixelSize: 24
                    font.weight: Font.DemiBold
                    font.letterSpacing: 1.5
                }
                Text {
                    text: Qt.formatDateTime(plugin.now, "dddd")
                    color: plugin.secondary
                    style: Text.Outline
                    styleColor: Qt.rgba(0, 0, 0, 0.28)
                    font.family: plugin.uiFont
                    font.pixelSize: 13
                    font.letterSpacing: 1.3
                }
            }
        }
    }
}
