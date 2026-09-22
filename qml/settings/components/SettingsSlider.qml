import QtQuick
import QtQuick.Layouts
import "../../components"
import "../../style"

// One labelled slider row: label and value on the first line, track below.
// PreferenceSlider and the audio volume controls both build on this, so a
// slider looks and behaves the same on every settings page.
ColumnLayout {
    id: row
    property string label: ""
    property real value: 0
    property real minimum: 0
    property real maximum: 100
    property real step: 1
    property string suffix: ""
    signal moved(real value)

    Layout.fillWidth: true
    spacing: 4

    RowLayout {
        Layout.fillWidth: true
        Text { Layout.minimumWidth: 0;
            Layout.fillWidth: true
            text: row.label
            color: Theme.text
            font.family: Theme.font
            font.pixelSize: 13
            elide: Text.ElideRight
        }
        Text {
            text: Number(row.value.toPrecision(12)) + row.suffix
            color: Theme.accent
            font.family: Theme.font
            font.pixelSize: 13
        }
    }
    SoftSlider {
        id: slider
        Layout.fillWidth: true
        from: row.minimum
        to: row.maximum
        stepSize: row.step
        Accessible.name: row.label
        Binding on value { value: row.value; when: !slider.pressed }
        onPressedChanged: if (!pressed) row.moved(value)
        Keys.onReleased: event => { if (event.key === Qt.Key_Left || event.key === Qt.Key_Right) row.moved(value) }
    }
}
