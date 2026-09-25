import QtQuick

Item {
    id: plugin
    required property var shell
    required property var settings
    required property var context
    implicitHeight: 48
    Text {
        anchors.centerIn: parent
        visible: plugin.settings.showGreeting
        text: "LunaDash"
        opacity: Number(plugin.settings.opacity ?? 1)
        color: (plugin.shell.state.appearance || {}).accent || "#9ccbfb"
        font.family: (plugin.shell.state.appearance || {}).fontFamily || Qt.application.font.family
    }
}
