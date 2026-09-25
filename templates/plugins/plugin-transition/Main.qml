import QtQuick

Item {
    id: plugin
    required property var shell
    required property var settings
    required property var context
    readonly property real cover: Math.max(0, 1 - Math.abs(2 * context.progress - 1))
    Rectangle {
        anchors.fill: parent
        color: plugin.context.background
        opacity: Math.min(1, plugin.cover * 1.7) * Number(plugin.settings.opacity ?? 1)
    }
    Rectangle {
        anchors.verticalCenter: parent.verticalCenter
        x: (parent.width + width) * plugin.context.progress - width
        width: parent.width * 0.5
        height: 3
        color: plugin.context.accent
        opacity: plugin.cover
    }
}
