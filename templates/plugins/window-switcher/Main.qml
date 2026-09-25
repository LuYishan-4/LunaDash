import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: plugin
    required property var shell
    required property var settings
    required property var context
    readonly property var selection: context.interaction || ({})
    Rectangle { anchors.fill: parent; color: plugin.context.background; opacity: 0.92 }
    MouseArea { anchors.fill: parent; onClicked: plugin.shell.command("switch-cancel", "") }
    ListView {
        id: list
        anchors.centerIn: parent
        width: Math.max(1, parent.width - 80)
        height: Math.min(300, parent.height - 80)
        orientation: ListView.Horizontal
        clip: true
        spacing: 16
        model: plugin.selection.windows || []
        currentIndex: Number(plugin.selection.index || 0)
        onCurrentIndexChanged: positionViewAtIndex(currentIndex, ListView.Contain)
        delegate: Rectangle {
            id: card
            objectName: "preview-card-" + modelData.id
            required property var modelData
            required property int index
            width: Math.min(300, list.width)
            height: list.height
            radius: 20
            color: plugin.context.background
            border.width: 2
            border.color: index === list.currentIndex ? plugin.context.accent : plugin.context.muted
            opacity: Number(plugin.settings.opacity ?? 1)
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 16
                Image {
                    Layout.fillWidth: true; Layout.fillHeight: true
                    source: card.modelData.thumbnail || ""
                    fillMode: Image.PreserveAspectFit
                    asynchronous: true
                }
                Text {
                    Layout.fillWidth: true
                    text: card.modelData.title || card.modelData.appId || ""
                    elide: Text.ElideRight
                    color: plugin.context.foreground
                    font.family: plugin.context.fontFamily
                }
            }
            MouseArea {
                anchors.fill: parent
                hoverEnabled: true
                onEntered: plugin.shell.command("switch-window", card.modelData.id)
                onClicked: {
                    plugin.shell.command("switch-window", card.modelData.id)
                    plugin.shell.command("switch-accept", "")
                }
            }
        }
    }
}
