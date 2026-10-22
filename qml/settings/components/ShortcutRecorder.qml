import QtQuick
import "../../style"

Rectangle {
    id: recorder
    required property var shell
    property string sequence: "Disabled"
    property bool recording: false
    signal accepted(string sequence)

    implicitWidth: 190
    implicitHeight: 40
    radius: 12
    activeFocusOnTab: true
    color: recording ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.18)
                     : mouse.containsMouse ? Theme.controlHover : Theme.control
    border.width: activeFocus || recording ? 1.5 : 1
    border.color: activeFocus || recording ? Theme.accent : Theme.border

    function keyName(key, text) {
        if (key === Qt.Key_Return || key === Qt.Key_Enter) return "Return"
        if (key === Qt.Key_Space) return "Space"
        if (key === Qt.Key_Tab) return "Tab"
        if (key === Qt.Key_Escape) return "Escape"
        if (key === Qt.Key_Backspace) return "Backspace"
        if (key === Qt.Key_Delete) return "Delete"
        if (key === Qt.Key_Left) return "Left"
        if (key === Qt.Key_Right) return "Right"
        if (key === Qt.Key_Up) return "Up"
        if (key === Qt.Key_Down) return "Down"
        if (key === Qt.Key_Plus) return "+"
        if (key === Qt.Key_Minus) return "-"
        if (key === Qt.Key_Equal) return "="
        if (key >= Qt.Key_0 && key <= Qt.Key_9) return String.fromCharCode(key)
        if (key >= Qt.Key_A && key <= Qt.Key_Z) return String.fromCharCode(key)
        if (text && text.length === 1) return text.toUpperCase()
        return ""
    }

    function begin() {
        recording = true
        recorder.shellCommand(true)
        forceActiveFocus()
    }

    function finish(value) {
        recording = false
        recorder.shellCommand(false)
        if (value !== undefined) recorder.accepted(value)
    }

    function shellCommand(enabled) {
        recorder.shell.command("shortcut-capture", enabled ? "true" : "false")
    }

    Keys.onPressed: event => {
        if (!recording) {
            if (event.key === Qt.Key_Return || event.key === Qt.Key_Space) begin()
            return
        }
        if (event.key === Qt.Key_Escape && !(event.modifiers & (Qt.MetaModifier | Qt.AltModifier))) {
            finish()
            event.accepted = true
            return
        }
        if (event.key === Qt.Key_Backspace && !(event.modifiers & (Qt.MetaModifier | Qt.AltModifier))) {
            finish("Disabled")
            event.accepted = true
            return
        }
        if (event.key === Qt.Key_Meta || event.key === Qt.Key_Shift ||
            event.key === Qt.Key_Control || event.key === Qt.Key_Alt)
            return
        const key = keyName(event.key, event.text)
        if (!key.length || !(event.modifiers & (Qt.MetaModifier | Qt.AltModifier))) {
            shake.restart()
            event.accepted = true
            return
        }
        let parts = []
        if (event.modifiers & Qt.MetaModifier) parts.push("Meta")
        if (event.modifiers & Qt.ControlModifier) parts.push("Ctrl")
        if (event.modifiers & Qt.AltModifier) parts.push("Alt")
        if (event.modifiers & Qt.ShiftModifier) parts.push("Shift")
        parts.push(key)
        finish(parts.join("+"))
        event.accepted = true
    }
    onActiveFocusChanged: if (!activeFocus && recording) finish()

    Row {
        anchors.centerIn: parent
        spacing: 8
        Text {
            text: recorder.recording ? recorder.shell.tr("Press shortcut…")
                : recorder.sequence === "Disabled" ? recorder.shell.tr("Disabled") : recorder.sequence
            color: recorder.recording ? Theme.accent : recorder.sequence === "Disabled" ? Theme.muted : Theme.text
            font.family: Theme.font
            font.pixelSize: 12
        }
        Text { visible: !recorder.recording; text: "⌨"; color: Theme.muted; font.family: Theme.font; font.pixelSize: 13 }
    }

    MouseArea { id: mouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: recorder.begin() }
    SequentialAnimation {
        id: shake
        NumberAnimation { target: recorder; property: "rotation"; to: -2; duration: 45 }
        NumberAnimation { target: recorder; property: "rotation"; to: 2; duration: 80 }
        NumberAnimation { target: recorder; property: "rotation"; to: 0; duration: 45 }
    }
    Behavior on color { ColorAnimation { duration: Theme.motion } }
    Behavior on border.color { ColorAnimation { duration: Theme.motion } }
}
