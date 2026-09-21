import QtQuick
import QtQuick.Layouts
import "../components"
import "../settings/components"
import "../style"

ColumnLayout {
    id: options
    required property var shell
    required property var schema
    required property var values
    signal edited(string key, var value)
    spacing: 12
    Repeater {
        model: Object.keys(options.schema)
        delegate: ColumnLayout {
            id: field
            required property string modelData
            readonly property var rule: options.schema[modelData]
            readonly property var value: options.values[modelData] ?? rule.default
            Layout.fillWidth: true
            RowLayout {
                Layout.fillWidth: true
                visible: field.rule.type === "boolean"
                Text {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    wrapMode: Text.Wrap
                    text: field.rule.label || field.modelData
                    color: Theme.text
                    font.family: Theme.font
                }
                SoftSwitch {
                    checked: Boolean(field.value)
                    onToggled: options.edited(field.modelData, checked)
                }
            }
            SettingsSlider {
                visible: field.rule.type === "number" || field.rule.type === "integer"
                label: field.rule.label || field.modelData
                value: Number(field.value)
                minimum: field.rule.minimum ?? 0
                maximum: field.rule.maximum ?? 100
                step: field.rule.type === "integer" ? 1 : 0.01
                onMoved: value => options.edited(field.modelData, field.rule.type === "integer" ? Math.round(value) : value)
            }
            RowLayout {
                Layout.fillWidth: true
                visible: field.rule.type === "string"
                Text {
                    text: field.rule.label || field.modelData
                    color: Theme.text
                    font.family: Theme.font
                }
                StyledComboBox {
                    visible: Boolean(field.rule.enum)
                    Layout.fillWidth: true
                    model: field.rule.enum || []
                    currentIndex: (field.rule.enum || []).indexOf(field.value)
                    onActivated: index => options.edited(field.modelData, field.rule.enum[index])
                }
                SoftField {
                    visible: !field.rule.enum
                    Layout.fillWidth: true
                    text: String(field.value)
                    onEditingFinished: options.edited(field.modelData, text)
                }
            }
        }
    }
}
