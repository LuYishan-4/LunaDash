import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../style"

Switch {
    id: control

    spacing: 10
    Layout.minimumWidth: indicator.width + spacing + 24
    implicitHeight: Math.max(38, contentItem.implicitHeight + topPadding + bottomPadding)
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus

    indicator: Rectangle {
        implicitWidth: 42
        implicitHeight: 24
        x: control.leftPadding
        y: control.height / 2 - height / 2
        radius: 12
        scale: control.down ? 0.96 : control.hovered ? 1.025 : 1
        color: control.checked
            ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b,
                      control.hovered ? 0.92 : 0.78)
            : control.hovered
                ? Qt.rgba(Theme.track.r, Theme.track.g, Theme.track.b, 0.96)
                : Theme.track
        border.width: control.activeFocus ? 2 : 1
        border.color: control.activeFocus
            ? Theme.focusRing
            : control.checked
                ? Qt.rgba(Theme.moon.r, Theme.moon.g, Theme.moon.b, 0.42)
                : Qt.rgba(Theme.starlight.r, Theme.starlight.g, Theme.starlight.b, 0.22)

        Rectangle {
            x: control.checked ? parent.width - width - 3 : 3
            y: 3
            width: 18
            height: 18
            radius: 9
            color: control.checked ? Theme.moon : Theme.knob
            border.width: 1
            border.color: Qt.rgba(Theme.starlight.r, Theme.starlight.g, Theme.starlight.b, 0.55)
            scale: control.down ? 0.92 : 1

            Rectangle {
                visible: control.checked
                width: 5
                height: 5
                radius: 2.5
                anchors.centerIn: parent
                color: Theme.secondaryAccent
                opacity: control.checked ? 1 : 0
            }

            Behavior on x { NumberAnimation { duration: Theme.motionFast; easing.type: Easing.OutCubic } }
            Behavior on scale { NumberAnimation { duration: Theme.motionFast; easing.type: Easing.OutCubic } }
        }

        Behavior on color { ColorAnimation { duration: Theme.motionFast } }
        Behavior on border.color { ColorAnimation { duration: Theme.motionFast } }
        Behavior on scale { NumberAnimation { duration: Theme.motionFast; easing.type: Easing.OutCubic } }
    }

    contentItem: Text {
        text: control.text
        wrapMode: Text.Wrap
        color: control.enabled
            ? control.hovered ? Theme.moon : Theme.text
            : Theme.muted
        font.family: Theme.font
        font.pixelSize: 13
        verticalAlignment: Text.AlignVCenter
        leftPadding: control.indicator.width + control.spacing
        Behavior on color { ColorAnimation { duration: Theme.motionFast } }
    }
}
