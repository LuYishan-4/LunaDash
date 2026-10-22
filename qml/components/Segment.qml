import QtQuick
import QtQuick.Layouts
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
    Layout.minimumWidth: 32
    implicitHeight: Math.max(32, label.implicitHeight + 14)
    radius: height / 2
    color: selected
        ? accentColor
        : mouse.containsMouse
            ? Theme.controlHover
            : fill
    scale: mouse.pressed ? 0.94 : mouse.containsMouse ? 1.025 : 1
    activeFocusOnTab: true
    border.width: activeFocus ? 1.5 : selected ? 0 : 1
    border.color: activeFocus
        ? Theme.focusRing
        : Qt.rgba(Theme.starlight.r, Theme.starlight.g, Theme.starlight.b, 0.18)

    Accessible.role: Accessible.Button
    Accessible.name: text
    Keys.onReturnPressed: clicked()
    Keys.onEnterPressed: clicked()
    Keys.onSpacePressed: clicked()

    Rectangle {
        visible: root.selected
        width: Math.max(12, root.width * 0.28)
        height: 2
        radius: 1
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 3
        color: Theme.accentInk
        opacity: 0.72
    }

    Text {
        id: label
        anchors.centerIn: parent
        width: Math.max(0, root.width - 28)
        wrapMode: Text.Wrap
        horizontalAlignment: Text.AlignHCenter
        text: root.text
        color: root.selected ? Theme.accentInk : root.ink
        font.family: Theme.font
        font.pixelSize: root.textSize
        font.weight: root.selected ? Font.DemiBold : Font.Medium
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }

    Behavior on color { ColorAnimation { duration: Theme.motionFast } }
    Behavior on scale { NumberAnimation { duration: Theme.motionFast; easing.type: Easing.OutCubic } }
    Behavior on implicitWidth { NumberAnimation { duration: Theme.motion; easing.type: Easing.OutCubic } }
    Behavior on border.color { ColorAnimation { duration: Theme.motionFast } }
}
