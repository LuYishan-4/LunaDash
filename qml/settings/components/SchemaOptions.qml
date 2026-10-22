import QtQuick
import QtQuick.Layouts
import "../../components"
import "../../style"

ColumnLayout {
    id: options
    required property var shell
    required property var schema
    required property var values
    signal edited(string key, var value)
    spacing: 12
    readonly property var keys: Object.keys(schema).sort((a, b) =>
        (schema[a].order ?? 0) - (schema[b].order ?? 0) || a.localeCompare(b))

    function widget(rule) {
        if (rule.control) return rule.control
        if (rule.enum) return "select"
        if (rule.type === "boolean") return "toggle"
        if (rule.type === "number" || rule.type === "integer")
            return rule.minimum !== undefined && rule.maximum !== undefined ? "slider" : "number"
        return "text"
    }
    Repeater {
        model: options.keys
        delegate: ColumnLayout {
            id: field
            objectName: "setting-field-" + modelData
            required property string modelData
            readonly property var rule: options.schema[modelData]
            readonly property var currentValue: options.values[modelData] ?? rule.default
            readonly property string controlType: options.widget(rule)
            readonly property string label: options.shell.tr(rule.label || modelData)
            readonly property real step: rule.step ?? (rule.type === "integer" ? 1 : 0.01)
            property string error: ""
            Layout.fillWidth: true
            enabled: !Boolean(rule.readOnly)
            spacing: 5

            function submitNumber(raw) {
                const text = String(raw).trim()
                const value = Number(text)
                const special = (rule.specialValues || []).indexOf(value) >= 0
                if (!text.length || !Number.isFinite(value) || Math.abs(value) > 9007199254740991 ||
                    (rule.type === "integer" && !Number.isInteger(value)) ||
                    (!special && ((rule.minimum !== undefined && value < rule.minimum) ||
                                  (rule.maximum !== undefined && value > rule.maximum)))) {
                    error = options.shell.tr("Enter a valid value within the allowed range.")
                    return
                }
                error = ""
                options.edited(modelData, value)
            }
            function stepNumber(direction) {
                const low = rule.minimum ?? -9007199254740991
                const high = rule.maximum ?? 9007199254740991
                const current = Number(currentValue)
                const next = Math.max(low, Math.min(high, Number((current + direction * step).toPrecision(15))))
                submitNumber(next)
            }
            RowLayout {
                visible: field.controlType !== "slider"
                Layout.fillWidth: true
                Text {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    text: field.label
                    wrapMode: Text.Wrap
                    color: Theme.text
                    font.family: Theme.font
                    font.pixelSize: 13
                }
                SoftSwitch {
                    objectName: "setting-toggle-" + field.modelData
                    visible: field.controlType === "toggle"
                    checked: Boolean(field.currentValue)
                    text: options.shell.tr(checked ? "Yes" : "No")
                    Accessible.name: field.label
                    onToggled: options.edited(field.modelData, checked)
                }
            }
            SettingsSlider {
                visible: field.controlType === "slider"
                label: field.label
                value: Number(field.currentValue)
                minimum: field.rule.minimum ?? 0
                maximum: field.rule.maximum ?? 100
                step: field.step
                onMoved: value => field.submitNumber(value)
            }
            RowLayout {
                visible: field.controlType === "number"
                Layout.fillWidth: true
                ShellButton {
                    text: "−"
                    Accessible.name: field.label + " " + options.shell.tr("Decrease")
                    onClicked: field.stepNumber(-1)
                }
                SoftField {
                    id: numberInput
                    objectName: "setting-number-" + field.modelData
                    Layout.fillWidth: true
                    text: String(field.currentValue)
                    clearButtonEnabled: false
                    invalid: field.error.length > 0
                    inputMethodHints: Qt.ImhFormattedNumbersOnly
                    Accessible.name: field.label
                    onEditingFinished: field.submitNumber(text)
                }
                ShellButton {
                    text: "+"
                    Accessible.name: field.label + " " + options.shell.tr("Increase")
                    onClicked: field.stepNumber(1)
                }
            }
            StyledComboBox {
                objectName: "setting-select-" + field.modelData
                visible: field.controlType === "select" && field.rule.type !== "array"
                Layout.fillWidth: true
                translationContext: options.shell
                model: (field.rule.enum || []).map(value => String(value))
                currentIndex: (field.rule.enum || []).indexOf(field.currentValue)
                Accessible.name: field.label
                onActivated: index => options.edited(field.modelData, field.rule.enum[index])
            }
            ColumnLayout {
                visible: field.controlType === "select" && field.rule.type === "array"
                Layout.fillWidth: true
                Repeater {
                    model: field.rule.type === "array" ? (field.rule.items.enum || []) : []
                    delegate: SoftSwitch {
                        required property var modelData
                        text: options.shell.tr(String(modelData))
                        checked: (field.currentValue || []).indexOf(modelData) >= 0
                        onToggled: {
                            let values = (field.currentValue || []).filter(value => value !== modelData)
                            if (checked) values.push(modelData)
                            options.edited(field.modelData, values)
                        }
                    }
                }
            }
            SoftField {
                visible: field.controlType === "text"
                Layout.fillWidth: true
                text: String(field.currentValue)
                Accessible.name: field.label
                onEditingFinished: options.edited(field.modelData, text)
            }
            HelpText {
                visible: Boolean(field.rule.description)
                shell: options.shell
                message: field.rule.description || ""
            }
            Text {
                visible: field.error.length > 0
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                text: field.error
                wrapMode: Text.Wrap
                color: Theme.danger
                font.family: Theme.font
                font.pixelSize: 12
            }
        }
    }
}
