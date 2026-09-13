import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../style"
import "../../components"
ColumnLayout {
    id: row
    required property var shell
    required property string preference
    required property string label
    property int minimum: 0
    property int maximum: 100
    property int step: 1
    property string suffix: ""
    spacing: 4; Layout.fillWidth: true
    RowLayout {
        Text { text: shell.tr(row.label); color: Theme.text; font.family: Theme.font; Layout.fillWidth: true }
        Text { text: ((shell.state.appearance || {})[row.preference] ?? row.minimum) + row.suffix; color: Theme.accent }
    }
    SoftSlider {
        id: slider
        Layout.fillWidth: true; from: row.minimum; to: row.maximum; stepSize: row.step
        Binding on value { value: (shell.state.appearance || {})[row.preference] ?? row.minimum; when: !slider.pressed }
        Accessible.name: shell.tr(row.label)
        onPressedChanged: if (!pressed) { const change = {}; change[row.preference] = Math.round(value); shell.setAppearance(change) }
        Keys.onReleased: event => { if (event.key === Qt.Key_Left || event.key === Qt.Key_Right) { const change = {}; change[row.preference] = Math.round(value); shell.setAppearance(change) } }
    }
}
