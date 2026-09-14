import QtQuick
import QtQuick.Layouts
import "../../style"

Rectangle {
    id: card
    property string title: ""
    property string description: ""
    default property alias content: body.data

    Layout.fillWidth: true
    implicitHeight: body.implicitHeight + 32
    radius: 16
    color: Qt.rgba(Theme.surface.r, Theme.surface.g, Theme.surface.b, 0.76)
    border.width: 1
    border.color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.18)

    ColumnLayout {
        id: body
        anchors.fill: parent
        anchors.margins: 16
        spacing: 10
        Text {
            visible: card.title.length > 0
            text: card.title
            color: Theme.text
            font.family: Theme.font
            font.pixelSize: 16
            font.bold: true
            Layout.fillWidth: true
        }
        Text {
            visible: card.description.length > 0
            text: card.description
            color: Theme.muted
            font.family: Theme.font
            font.pixelSize: 12
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }
    }

    Behavior on border.color { ColorAnimation { duration: Theme.motion } }
}
