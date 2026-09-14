import QtQuick
import "../components"
import "../style"

Rectangle {
    id: entry
    property string text: ""
    property string glyph: "general"
    property bool available: true
    signal triggered()

    implicitHeight: 34
    implicitWidth: 180
    radius: 8
    color: entryMouse.containsMouse && entry.available
        ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.18)
        : "transparent"

    Row {
        anchors.verticalCenter: parent.verticalCenter
        anchors.left: parent.left
        anchors.leftMargin: 11
        spacing: 10
        LineIcon {
            width: 16
            height: 16
            name: entry.glyph
            ink: entry.available ? Theme.text : Theme.muted
            opacity: entry.available ? 1 : 0.55
            anchors.verticalCenter: parent.verticalCenter
        }
        Text {
            text: entry.text
            color: entry.available ? Theme.text : Theme.muted
            font.family: Theme.font
            font.pixelSize: 12
            anchors.verticalCenter: parent.verticalCenter
        }
    }

    MouseArea {
        id: entryMouse
        anchors.fill: parent
        hoverEnabled: true
        enabled: entry.available
        cursorShape: Qt.PointingHandCursor
        onClicked: entry.triggered()
    }

    Behavior on color { ColorAnimation { duration: Math.min(Theme.motion, 120) } }
}
