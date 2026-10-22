import QtQuick
import QtQuick.Controls
import "../style"

Slider {
    id: control

    property bool accentFill: true
    property string displayValue: Math.round(value).toString()

    implicitHeight: 34
    focusPolicy: Qt.StrongFocus

    background: Rectangle {
        x: control.leftPadding
        y: control.topPadding + control.availableHeight / 2 - height / 2
        width: control.availableWidth
        height: sliderHover.hovered || control.activeFocus ? 6 : 4
        radius: height / 2
        color: Theme.track

        Rectangle {
            width: control.visualPosition * parent.width
            height: parent.height
            radius: parent.radius
            color: control.accentFill ? Theme.accent : Theme.starlight
            Behavior on width { NumberAnimation { duration: Theme.motionFast; easing.type: Easing.OutCubic } }
        }

        Behavior on height { NumberAnimation { duration: Theme.motionFast } }
    }

    handle: Rectangle {
        x: control.leftPadding + control.visualPosition * (control.availableWidth - width)
        y: control.topPadding + control.availableHeight / 2 - height / 2
        width: control.pressed ? 21 : sliderHover.hovered || control.activeFocus ? 19 : 16
        height: width
        radius: width / 2
        color: Theme.moon
        border.color: control.activeFocus ? Theme.accent : Theme.focusRing
        border.width: control.activeFocus ? 2 : 1
        scale: control.pressed ? 0.92 : 1

        Rectangle {
            width: 5
            height: 5
            radius: 2.5
            anchors.centerIn: parent
            color: Theme.secondaryAccent
        }

        Rectangle {
            visible: control.pressed || control.activeFocus
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.top
            anchors.bottomMargin: 7
            width: valueText.implicitWidth + 14
            height: 24
            radius: 9
            color: Theme.surfaceOpaque
            border.width: 1
            border.color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.42)
            opacity: visible ? 1 : 0

            Text {
                id: valueText
                anchors.centerIn: parent
                text: control.displayValue
                color: Theme.text
                font.family: Theme.font
                font.pixelSize: 10
                font.weight: Font.DemiBold
            }
        }

        Behavior on width { NumberAnimation { duration: Theme.motionFast; easing.type: Easing.OutCubic } }
        Behavior on scale { NumberAnimation { duration: Theme.motionFast; easing.type: Easing.OutCubic } }
        Behavior on border.color { ColorAnimation { duration: Theme.motionFast } }
    }

    HoverHandler { id: sliderHover }
}
