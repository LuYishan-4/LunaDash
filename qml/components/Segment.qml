import QtQuick
import "../style"
Item {
    id: root
    property string text: ""
    property color fill: Theme.surface
    property color ink: Theme.text
    property bool selected: false
    signal clicked()
    implicitWidth: Math.max(28, label.implicitWidth + 30)
    implicitHeight: Theme.barHeight
    activeFocusOnTab: true
    Accessible.role: Accessible.Button
    Accessible.name: text
    Keys.onReturnPressed: clicked()
    Keys.onSpacePressed: clicked()
    onFillChanged: shape.requestPaint()
    onSelectedChanged: shape.requestPaint()
    Canvas {
        id: shape
        anchors.fill: parent
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
        onPaint: {
            const c = getContext("2d"); c.reset()
            c.fillStyle = root.selected || mouse.containsMouse ? Theme.accent : root.fill
            c.beginPath(); c.moveTo(0, height / 2); c.lineTo(10, 0)
            c.lineTo(width - 10, 0); c.lineTo(width, height / 2)
            c.lineTo(width - 10, height); c.lineTo(10, height); c.closePath(); c.fill()
        }
    }
    Text { id: label; anchors.centerIn: parent; text: root.text; color: root.selected || mouse.containsMouse ? "#182526" : root.ink; font.family: Theme.font; font.pixelSize: 12 }
    MouseArea { id: mouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: root.clicked(); onContainsMouseChanged: shape.requestPaint() }
}
