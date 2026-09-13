import QtQuick
import "../style"
Rectangle {
    id: root
    property string text: ""
    property bool active: false
    signal clicked()
    implicitWidth: Math.max(36, label.implicitWidth + 26)
    implicitHeight: 36
    radius: Math.min(12, height / 2)
    color: active ? Theme.accent : mouse.containsMouse ? "#384a5b" : "#25313e"
    scale: mouse.pressed ? 0.96 : 1
    opacity: enabled ? 1 : 0.4
    border.width: activeFocus ? 1 : 0
    border.color: Theme.accent
    activeFocusOnTab: true
    Accessible.role: Accessible.Button; Accessible.name: text
    Keys.onReturnPressed: clicked(); Keys.onSpacePressed: clicked()
    Behavior on color { ColorAnimation { duration: Theme.motion } }
    Behavior on scale { NumberAnimation { duration: Math.min(Theme.motion, 150); easing.type: Easing.OutCubic } }
    Text { id: label; anchors.centerIn: parent; text: root.text; color: root.active ? "#102133" : Theme.text; font.family: Theme.font; font.pixelSize: 12 }
    MouseArea { id: mouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: root.clicked() }
}
