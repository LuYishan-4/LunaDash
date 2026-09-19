import QtQuick
import QtQuick.Controls
import "../style"

TextField {
    id: control

    property bool invalid: false
    property bool clearButtonEnabled: true
    property string helperText: ""

    implicitHeight: 40
    leftPadding: 16
    rightPadding: clearButton.visible ? 42 : 16
    color: Theme.text
    placeholderTextColor: Qt.rgba(Theme.muted.r, Theme.muted.g, Theme.muted.b, 0.82)
    font.family: Theme.font
    font.pixelSize: 12
    selectByMouse: true
    persistentSelection: true
    focusPolicy: Qt.StrongFocus
    activeFocusOnPress: true

    function prepareInputMethod() {
        if (!activeFocus || !enabled || readOnly)
            return
        Qt.callLater(function() {
            if (control.activeFocus)
                Qt.inputMethod.update(Qt.ImQueryAll)
        })
    }

    onActiveFocusChanged: if (activeFocus) prepareInputMethod()
    Component.onCompleted: if (activeFocus) prepareInputMethod()

    TapHandler {
        acceptedButtons: Qt.LeftButton
        onPressedChanged: {
            if (!pressed)
                return
            control.forceActiveFocus(Qt.MouseFocusReason)
            control.prepareInputMethod()
        }
    }

    background: Rectangle {
        radius: 13
        color: control.activeFocus
            ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.11)
            : fieldHover.hovered
                ? Theme.surfaceElevated
                : Theme.surface
        border.width: control.activeFocus || control.invalid ? 2 : 1
        border.color: control.invalid
            ? Theme.danger
            : control.activeFocus
                ? Theme.moon
                : fieldHover.hovered
                    ? Qt.rgba(Theme.starlight.r, Theme.starlight.g, Theme.starlight.b, 0.46)
                    : Qt.rgba(Theme.starlight.r, Theme.starlight.g, Theme.starlight.b, 0.28)

        Rectangle {
            width: control.activeFocus ? 6 : 4
            height: width
            radius: width / 2
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.rightMargin: 10
            anchors.topMargin: 8
            color: control.invalid
                ? Theme.danger
                : Qt.rgba(Theme.starlight.r, Theme.starlight.g, Theme.starlight.b,
                           control.activeFocus ? 0.92 : 0.28)
            Behavior on width { NumberAnimation { duration: Theme.motionFast } }
            Behavior on color { ColorAnimation { duration: Theme.motionFast } }
        }

        Behavior on color { ColorAnimation { duration: Theme.motionFast } }
        Behavior on border.color { ColorAnimation { duration: Theme.motionFast } }
    }

    Item {
        id: clearButton
        visible: control.clearButtonEnabled && control.text.length > 0 && !control.readOnly
        width: 30
        height: 30
        anchors.right: parent.right
        anchors.rightMargin: 5
        anchors.verticalCenter: parent.verticalCenter
        opacity: clearMouse.containsMouse ? 1 : 0.72
        scale: clearMouse.pressed ? 0.9 : 1

        Rectangle {
            anchors.fill: parent
            radius: height / 2
            color: clearMouse.containsMouse
                ? Qt.rgba(Theme.starlight.r, Theme.starlight.g, Theme.starlight.b, 0.12)
                : "transparent"
        }

        Text {
            anchors.centerIn: parent
            text: "×"
            color: Theme.muted
            font.family: Theme.font
            font.pixelSize: 15
        }

        MouseArea {
            id: clearMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: {
                control.clear()
                control.forceActiveFocus(Qt.MouseFocusReason)
                control.prepareInputMethod()
            }
        }

        Behavior on opacity { NumberAnimation { duration: Theme.motionFast } }
        Behavior on scale { NumberAnimation { duration: Theme.motionFast } }
    }

    HoverHandler { id: fieldHover }
}
