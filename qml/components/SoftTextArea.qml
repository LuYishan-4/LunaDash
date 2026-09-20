import QtQuick
import QtQuick.Controls
import "../style"

TextArea {
    id: control
    property bool invalid: false
    property int characterLimit: 0
    readonly property int characterCount: text.length

    color: Theme.text
    placeholderTextColor: Theme.muted
    font.family: Theme.font
    font.pixelSize: 12
    selectByMouse: true
    persistentSelection: true
    activeFocusOnTab: true
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
    onTextChanged: if (characterLimit > 0 && text.length > characterLimit) text = text.slice(0, characterLimit)
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
        color: control.activeFocus
            ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.10)
            : areaHover.hovered ? Theme.surfaceElevated : Theme.surface
        border.width: control.activeFocus || control.invalid ? 2 : 1
        border.color: control.invalid
            ? Theme.danger
            : control.activeFocus
                ? Theme.moon
                : areaHover.hovered
                    ? Qt.rgba(Theme.starlight.r, Theme.starlight.g, Theme.starlight.b, 0.44)
                    : Theme.border
        Behavior on color { ColorAnimation { duration: Theme.motionFast } }
        Behavior on border.color { ColorAnimation { duration: Theme.motionFast } }
    }

    Text {
        visible: control.characterLimit > 0
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.rightMargin: 10
        anchors.bottomMargin: 7
        text: control.characterCount + "/" + control.characterLimit
        color: control.characterCount >= control.characterLimit ? Theme.warning : Theme.muted
        font.family: Theme.font
        font.pixelSize: 9
    }

    HoverHandler { id: areaHover }
}
