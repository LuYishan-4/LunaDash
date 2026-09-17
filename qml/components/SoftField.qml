import QtQuick
import QtQuick.Controls
import "../style"

TextField {
    id: control
    property bool invalid: false

    implicitHeight: 40
    leftPadding: 16
    rightPadding: 16
    color: Theme.text
    placeholderTextColor: Theme.muted
    font.family: Theme.font
    font.pixelSize: 12
    selectByMouse: true
    persistentSelection: true
    focusPolicy: Qt.StrongFocus
    activeFocusOnPress: true

    function prepareInputMethod() {
        if (!activeFocus || !enabled || readOnly)
            return
        // Notify Qt/Fcitx as soon as focus enters the field. Waiting until the
        // first physical key causes the first character to feel delayed while
        // the input context activates.
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
            ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.10)
            : Theme.surface
        border.width: control.activeFocus || control.invalid ? 2 : 1
        border.color: control.invalid
            ? Theme.danger
            : control.activeFocus
                ? Theme.moon
                : Qt.rgba(Theme.starlight.r, Theme.starlight.g, Theme.starlight.b, 0.28)

        Rectangle {
            width: 4
            height: 4
            radius: 2
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.rightMargin: 10
            anchors.topMargin: 8
            color: Qt.rgba(Theme.starlight.r, Theme.starlight.g, Theme.starlight.b,
                           control.activeFocus ? 0.82 : 0.28)
            Behavior on color { ColorAnimation { duration: Theme.motion } }
        }

        Behavior on color { ColorAnimation { duration: Theme.motion } }
        Behavior on border.color { ColorAnimation { duration: Theme.motion } }
    }
}
