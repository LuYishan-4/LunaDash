import QtQuick
import QtQuick.Controls
import "../style"

TextArea {
    id: control
    property bool invalid: false

    color: Theme.text
    placeholderTextColor: Theme.muted
    font.family: Theme.font
    font.pixelSize: 12
    selectByMouse: true
    persistentSelection: true
    focusPolicy: Qt.StrongFocus
    activeFocusOnPress: true
    leftPadding: 14
    rightPadding: 14
    topPadding: 12
    bottomPadding: 12

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
        radius: 12
        color: Theme.surface
        border.width: control.activeFocus || control.invalid ? 2 : 1
        border.color: control.invalid
            ? Theme.danger
            : control.activeFocus
                ? Theme.moon
                : Theme.border
        Behavior on border.color { ColorAnimation { duration: Theme.motion } }
    }
}
