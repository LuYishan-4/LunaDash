import QtQuick
import "../components"
import "../style"

Rectangle {
    id: entry

    property string text: ""
    property string glyph: "general"
    property bool available: true
    signal triggered()

    implicitHeight: 36
    implicitWidth: 190
    radius: 9
    scale: entryMouse.pressed ? 0.985 : entryMouse.containsMouse && available ? 1.01 : 1
    color: entryMouse.containsMouse && entry.available
        ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.18)
        : "transparent"

    Rectangle {
        visible: entryMouse.containsMouse && entry.available
        width: 3
        height: 16
        radius: 1.5
        anchors.left: parent.left
        anchors.leftMargin: 3
        anchors.verticalCenter: parent.verticalCenter
        color: Theme.accent
    }

    Row {
        anchors.verticalCenter: parent.verticalCenter
        anchors.left: parent.left
        anchors.leftMargin: entryMouse.containsMouse && entry.available ? 14 : 11
        spacing: 10

        LineIcon {
            width: 16
            height: 16
            name: entry.glyph
            ink: entry.available
                ? entryMouse.containsMouse ? Theme.accent : Theme.text
                : Theme.muted
            opacity: entry.available ? 1 : 0.55
            anchors.verticalCenter: parent.verticalCenter
            Behavior on ink { ColorAnimation { duration: Theme.motionFast } }
        }

        Text {
            text: entry.text
            color: entry.available
                ? entryMouse.containsMouse ? Theme.moon : Theme.text
                : Theme.muted
            font.family: Theme.font
            font.pixelSize: 12
            font.weight: entryMouse.containsMouse ? Font.DemiBold : Font.Normal
            anchors.verticalCenter: parent.verticalCenter
            Behavior on color { ColorAnimation { duration: Theme.motionFast } }
        }

        Behavior on anchors.leftMargin {
            NumberAnimation { duration: Theme.motionFast; easing.type: Easing.OutCubic }
        }
    }

    MouseArea {
        id: entryMouse
        anchors.fill: parent
        hoverEnabled: true
        enabled: entry.available
        cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
        onClicked: entry.triggered()
    }

    Behavior on color { ColorAnimation { duration: Theme.motionFast } }
    Behavior on scale { NumberAnimation { duration: Theme.motionFast; easing.type: Easing.OutCubic } }
}
