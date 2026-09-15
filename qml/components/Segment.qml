import QtQuick
import "../style"
Rectangle {
    id: root
    property string text: ""
    property color fill: "transparent"
    property color ink: Theme.text
    property color accentColor: Theme.accent
    property int textSize: 13
    property bool selected: false
    signal clicked()
    implicitWidth: Math.max(32, label.implicitWidth + 28)
    implicitHeight: 32
    radius: height / 2
    color: selected ? accentColor : mouse.containsMouse ? Theme.controlHover : fill
    scale: mouse.pressed ? 0.93 : 1
    activeFocusOnTab: true
    border.width: activeFocus ? 1 : 0; border.color: accentColor
    Accessible.role: Accessible.Button; Accessible.name: text
    Keys.onReturnPressed: clicked(); Keys.onSpacePressed: clicked()
    Behavior on color { ColorAnimation { duration: Theme.motion } }
    Behavior on scale { NumberAnimation { duration: Math.min(Theme.motion, 150); easing.type: Easing.OutCubic } }
    Behavior on implicitWidth { NumberAnimation { duration: Theme.motion; easing.type: Easing.OutCubic } }
    Text { id: label; anchors.centerIn: parent; text: root.text; color: root.selected ? Theme.accentInk : root.ink; font.family: Theme.font; font.pixelSize: root.textSize; font.weight: Font.Medium }
    MouseArea { id: mouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: root.clicked() }
}
