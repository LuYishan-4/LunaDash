import QtQuick
import "../style"
Rectangle {
    id: root
    property string text: ""
    property bool active: false
    signal clicked()
    implicitWidth: Math.max(36, label.implicitWidth + 28)
    implicitHeight: 36
    radius: Math.min(13, height / 2)
    color: active ? Theme.accent : mouse.containsMouse ? Theme.controlHover : Theme.control
    scale: mouse.pressed ? 0.96 : 1
    opacity: enabled ? 1 : 0.4
    border.width: activeFocus || mouse.containsMouse ? 1 : 0
    border.color: activeFocus ? Theme.moon : Qt.rgba(Theme.starlight.r, Theme.starlight.g, Theme.starlight.b, 0.46)
    activeFocusOnTab: true
    Accessible.role: Accessible.Button; Accessible.name: text
    Keys.onReturnPressed: clicked(); Keys.onSpacePressed: clicked()
    Behavior on color { ColorAnimation { duration: Theme.motion } }
    Behavior on scale { NumberAnimation { duration: Math.min(Theme.motion, 150); easing.type: Easing.OutCubic } }
    Behavior on border.color { ColorAnimation { duration: Theme.motion } }

    Rectangle {
        visible: !root.active
        width: 4
        height: 4
        radius: 2
        anchors.left: parent.left
        anchors.leftMargin: 8
        anchors.verticalCenter: parent.verticalCenter
        color: Qt.rgba(Theme.starlight.r, Theme.starlight.g, Theme.starlight.b, mouse.containsMouse ? 0.8 : 0.34)
        Behavior on color { ColorAnimation { duration: Theme.motion } }
    }

    Text { id: label; anchors.centerIn: parent; text: root.text; color: root.active ? Theme.accentInk : Theme.text; font.family: Theme.font; font.pixelSize: 12 }
    MouseArea { id: mouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: root.clicked() }
}
