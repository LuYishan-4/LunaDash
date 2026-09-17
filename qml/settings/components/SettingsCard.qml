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
    radius: 18
    color: Qt.rgba(Theme.surface.r, Theme.surface.g, Theme.surface.b, 0.82)
    border.width: 1
    border.color: Qt.rgba(Theme.starlight.r, Theme.starlight.g, Theme.starlight.b, 0.24)

    Rectangle {
        width: 34
        height: 34
        radius: 17
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 12
        color: Qt.rgba(Theme.moon.r, Theme.moon.g, Theme.moon.b, 0.08)
        Rectangle {
            width: 32
            height: 32
            radius: 16
            x: 9
            y: -5
            color: card.color
        }
    }

    Repeater {
        model: [[0.12, 0.22], [0.76, 0.74], [0.56, 0.18]]
        Rectangle {
            required property var modelData
            width: 3
            height: 3
            radius: 1.5
            x: card.width * modelData[0]
            y: card.height * modelData[1]
            color: Qt.rgba(Theme.starlight.r, Theme.starlight.g, Theme.starlight.b, 0.26)
        }
    }

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
