import QtQuick
import "../style"
Rectangle {
    id: root
    property string text: ""
    property bool active: false
    signal clicked()
    implicitWidth: Math.max(34, label.implicitWidth + 22)
    implicitHeight: 32
    radius: 3
    color: mouse.containsMouse ? "#40575a" : active ? "#365456" : "#243234"
    border.width: active || activeFocus ? 1 : 0
    border.color: Theme.accent
    activeFocusOnTab: true
    Accessible.role: Accessible.Button
    Accessible.name: text
    Keys.onReturnPressed: clicked()
    Keys.onSpacePressed: clicked()
    Text { id: label; anchors.centerIn: parent; text: root.text; color: root.active ? Theme.accent : Theme.text; font.family: Theme.font; font.pixelSize: 12 }
    MouseArea { id: mouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: root.clicked() }
}
