import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../style"

Rectangle {
    id: root

    property string text: ""
    property string iconName: ""
    property string toolTip: ""
    property bool active: false
    property bool busy: false
    property bool destructive: false
    property bool quiet: false

    signal clicked()

    Layout.minimumWidth: 36
    implicitWidth: Math.max(36, label.implicitWidth + (root.iconName.length || root.busy ? 22 : 0) + 28)
    implicitHeight: Math.max(36, label.implicitHeight + 16)
    radius: Math.min(13, height / 2)
    color: {
        if (root.active)
            return root.destructive ? Theme.danger : Theme.accent
        if (mouse.containsMouse)
            return root.quiet ? Qt.rgba(Theme.starlight.r, Theme.starlight.g, Theme.starlight.b, 0.10) : Theme.controlHover
        return root.quiet ? "transparent" : Theme.control
    }
    scale: mouse.pressed ? 0.96 : mouse.containsMouse ? 1.018 : 1
    opacity: enabled ? 1 : 0.42
    border.width: activeFocus || mouse.containsMouse ? 1 : 0
    border.color: activeFocus
        ? Theme.focusRing
        : root.destructive
            ? Qt.rgba(Theme.danger.r, Theme.danger.g, Theme.danger.b, 0.58)
            : Qt.rgba(Theme.starlight.r, Theme.starlight.g, Theme.starlight.b, 0.46)
    activeFocusOnTab: true

    Accessible.role: Accessible.Button
    Accessible.name: text
    Accessible.description: toolTip

    Keys.onReturnPressed: if (enabled && !busy) clicked()
    Keys.onEnterPressed: if (enabled && !busy) clicked()
    Keys.onSpacePressed: if (enabled && !busy) clicked()

    Rectangle {
        anchors.fill: parent
        anchors.margins: -3
        radius: root.radius + 3
        color: "transparent"
        border.width: root.activeFocus ? 1 : 0
        border.color: Qt.rgba(Theme.focusRing.r, Theme.focusRing.g, Theme.focusRing.b, 0.42)
        opacity: root.activeFocus ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: Theme.motionFast } }
    }

    Row {
        id: content
        anchors.centerIn: parent
        width: Math.max(0, root.width - 28)
        spacing: 7

        LineIcon {
            visible: root.iconName.length > 0 && !root.busy
            width: visible ? 15 : 0
            height: 15
            anchors.verticalCenter: parent.verticalCenter
            name: root.iconName
            ink: root.active
                ? Theme.accentInk
                : root.destructive
                    ? Theme.danger
                    : Theme.text
        }

        LineIcon {
            visible: root.busy
            width: visible ? 15 : 0
            height: 15
            anchors.verticalCenter: parent.verticalCenter
            name: "update"
            ink: root.active ? Theme.accentInk : Theme.moon
            RotationAnimation on rotation {
                running: root.busy && Theme.animations
                loops: Animation.Infinite
                from: 0
                to: 360
                duration: 950
            }
        }

        Text {
            id: label
            anchors.verticalCenter: parent.verticalCenter
            width: Math.max(0, content.width - (root.iconName.length || root.busy ? 22 : 0))
            wrapMode: Text.Wrap
            horizontalAlignment: Text.AlignHCenter
            text: root.text
            color: root.active
                ? Theme.accentInk
                : root.destructive
                    ? Theme.danger
                    : Theme.text
            font.family: Theme.font
            font.pixelSize: 12
            font.weight: root.active ? Font.DemiBold : Font.Medium
        }
    }

    Rectangle {
        visible: !root.active && !root.quiet
        width: mouse.containsMouse ? 5 : 4
        height: width
        radius: width / 2
        anchors.left: parent.left
        anchors.leftMargin: 8
        anchors.verticalCenter: parent.verticalCenter
        color: Qt.rgba(Theme.starlight.r, Theme.starlight.g, Theme.starlight.b,
                       mouse.containsMouse ? 0.84 : 0.34)
        Behavior on width { NumberAnimation { duration: Theme.motionFast } }
        Behavior on color { ColorAnimation { duration: Theme.motionFast } }
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        enabled: root.enabled && !root.busy
        cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
        onClicked: root.clicked()
    }

    ToolTip.visible: root.toolTip.length > 0 && mouse.containsMouse
    ToolTip.delay: 500
    ToolTip.text: root.toolTip

    Behavior on color { ColorAnimation { duration: Theme.motionFast } }
    Behavior on scale { NumberAnimation { duration: Theme.motionFast; easing.type: Easing.OutCubic } }
    Behavior on border.color { ColorAnimation { duration: Theme.motionFast } }
    Behavior on opacity { NumberAnimation { duration: Theme.motionFast } }
}
