import QtQuick
import QtQuick.Layouts
import Quickshell
import Quickshell.Wayland

Item {
    id: plugin
    property var shell: null
    width: 0
    height: 0

    readonly property var appearance: shell ? (shell.state.appearance || {}) : ({})
    readonly property color accent: appearance.accent || "#9ccbfb"
    readonly property color primary: Qt.lighter(accent, 1.08)
    readonly property color secondary: Qt.lighter(accent, 1.35)
    readonly property color surface: Qt.rgba(0.055, 0.075, 0.12, 0.78)
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
        anchors.left: true
        anchors.top: true
        margins.left: Math.max(24, Math.round(((screen ? screen.width : 1440) - implicitWidth) / 2))
        margins.top: Math.max(80, Math.round(((screen ? screen.height : 900) - implicitHeight) / 2))
        implicitWidth: 500
        implicitHeight: 164
        exclusiveZone: 0
        color: "transparent"
        WlrLayershell.layer: WlrLayer.Bottom
        WlrLayershell.namespace: "lunadash-plugin-digital-clock"
        WlrLayershell.keyboardFocus: WlrKeyboardFocus.None

        Rectangle {
            anchors.fill: parent
            radius: 30
            color: plugin.surface
            border.width: 1
            border.color: Qt.rgba(plugin.accent.r, plugin.accent.g, plugin.accent.b, 0.30)

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 30
                anchors.rightMargin: 30
                anchors.topMargin: 24
                anchors.bottomMargin: 24
                spacing: 24

                RowLayout {
                    spacing: 4
                    Text { text: Qt.formatDateTime(plugin.now, "HH"); color: plugin.primary; font.family: plugin.uiFont; font.pixelSize: 72; font.weight: Font.Bold }
                    Text { text: ":"; color: plugin.accent; opacity: 0.78; font.family: plugin.uiFont; font.pixelSize: 68; Layout.alignment: Qt.AlignTop; Layout.topMargin: -5 }
                    Text { text: Qt.formatDateTime(plugin.now, "mm"); color: plugin.secondary; font.family: plugin.uiFont; font.pixelSize: 72; font.weight: Font.Bold }
                }

                Rectangle { Layout.fillHeight: true; Layout.preferredWidth: 4; Layout.topMargin: 8; Layout.bottomMargin: 8; radius: 2; color: plugin.accent; opacity: 0.72 }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0
                    Text { text: Qt.formatDateTime(plugin.now, "MMMM").toUpperCase(); color: plugin.secondary; font.family: plugin.uiFont; font.pixelSize: 16; font.weight: Font.Bold; font.letterSpacing: 3 }
                    Text { text: Qt.formatDateTime(plugin.now, "dd"); color: plugin.primary; font.family: plugin.uiFont; font.pixelSize: 35; font.weight: Font.DemiBold; font.letterSpacing: 2 }
                    Text { text: Qt.formatDateTime(plugin.now, "dddd"); color: plugin.secondary; font.family: plugin.uiFont; font.pixelSize: 14; font.letterSpacing: 1.5 }
                }
            }
        }
    }
}
