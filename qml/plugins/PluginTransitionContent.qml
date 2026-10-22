import QtQuick
import "../style"

// The reusable built-in full-screen curtain. Progress is supplied by the host.
Item {
    id: curtain
    required property real progress
    property string label: ""
    readonly property real cover: Math.max(0, 1 - Math.abs(2 * progress - 1))
    Rectangle {
        anchors.fill: parent
        color: Theme.background
        opacity: Math.min(1, curtain.cover * 1.7)
    }
    Rectangle {
        anchors.verticalCenter: parent.verticalCenter
        x: (parent.width + width) * curtain.progress - width
        width: parent.width * 0.45
        height: 2
        radius: 1
        color: Theme.accent
        opacity: curtain.cover
    }
    Text {
        anchors.centerIn: parent
        width: Math.min(parent.width - 48, 480)
        text: curtain.label
        color: Theme.text
        font.family: Theme.font
        font.pixelSize: 20
        horizontalAlignment: Text.AlignHCenter
        elide: Text.ElideRight
        opacity: curtain.cover
        scale: 0.97 + 0.03 * curtain.cover
    }
}
