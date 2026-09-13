import QtQuick
import "../style"
Rectangle {
    id: root
    property string text: ""
    property color fill: "transparent"
    property color ink: Theme.text
    property bool selected: false
    signal clicked()
    implicitWidth: Math.max(32, label.implicitWidth + 28)
    implicitHeight: Theme.barHeight - 8
    radius: height / 2
    color: selected ? Theme.accent : mouse.containsMouse ? "#344555" : fill
    scale: mouse.pressed ? 0.93 : 1
    activeFocusOnTab: true
    border.width: activeFocus ? 1 : 0; border.color: Theme.accent
    Accessible.role: Accessible.Button; Accessible.name: text
    Keys.onReturnPressed: clicked(); Keys.onSpacePressed: clicked()
    Behavior on color { ColorAnimation { duration: Theme.motion } }
    Behavior on scale { NumberAnimation { duration: Math.min(Theme.motion, 150); easing.type: Easing.OutCubic } }
    Behavior on implicitWidth { NumberAnimation { duration: Theme.motion; easing.type: Easing.OutCubic } }
    Text { id: label; anchors.centerIn: parent; text: root.text; color: root.selected ? "#102133" : root.ink; font.family: Theme.font; font.pixelSize: 13; font.weight: Font.Medium }
    MouseArea { id: mouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: root.clicked() }
}
