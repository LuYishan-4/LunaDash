import QtQuick
import QtQuick.Layouts

// Appearance preference slider. The row itself is SettingsSlider, so every
// slider on a settings page shares the same label, value and track styling.
ColumnLayout {
    id: row
    required property var shell
    required property string preference
    property string label: ""
    property int minimum: 0
    property int maximum: 100
    property int step: 1
    property string suffix: ""

    Layout.fillWidth: true

    SettingsSlider {
        Layout.fillWidth: true
        label: row.shell.tr(row.label)
        value: (row.shell.state.appearance || {})[row.preference] ?? row.minimum
        minimum: row.minimum
        maximum: row.maximum
        step: row.step
        suffix: row.suffix
        onMoved: value => {
            const change = {}
            change[row.preference] = Math.round(value)
            row.shell.setAppearance(change)
        }
    }
}
