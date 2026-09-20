import QtQuick

Item {
    id: plugin
    required property var shell
    required property var settings
    required property var context
    implicitHeight: 48
    Text {
        anchors.centerIn: parent
        text: plugin.settings.text
        color: (plugin.shell.state.appearance || {}).accent || "#9ccbfb"
        font.family: (plugin.shell.state.appearance || {}).fontFamily || Qt.application.font.family
    }
}
