import QtQuick
import QtTest
import "../../qml/settings/components"
import "../../qml/style"

TestCase {
    name: "SettingsSchemaControls"
    when: windowShown
    width: 600
    height: 800
    QtObject { id: fakeShell; function tr(text) { return "translated " + text } }
    SchemaOptions {
        id: options
        width: 500
        shell: fakeShell
        schema: ({
            active: {type: "boolean", default: true},
            choice: {type: "integer", default: 2, enum: [1, 2, 4]},
            duration: {type: "integer", control: "number", default: 200, minimum: 0, maximum: 600},
            strength: {type: "number", default: 0.82, minimum: 0, maximum: 1, step: 0.01}
        })
        values: ({})
    }
    SignalSpy { id: edits; target: options; signalName: "edited" }
    function initTestCase() { Theme.animations = false }
    function init() { edits.clear() }
    function test_control_inference() {
        compare(options.widget(options.schema.active), "toggle")
        compare(options.widget(options.schema.choice), "select")
        compare(options.widget(options.schema.duration), "number")
        compare(options.widget(options.schema.strength), "slider")
    }
    function test_enum_keeps_numeric_storage() {
        const combo = findChild(options, "setting-select-choice")
        verify(combo !== null)
        combo.activated(2)
        compare(edits.count, 1)
        compare(edits.signalArguments[0][0], "choice")
        compare(edits.signalArguments[0][1], 4)
        compare(typeof edits.signalArguments[0][1], "number")
    }
    function test_number_rejects_invalid_values() {
        const field = findChild(options, "setting-field-duration")
        verify(field !== null)
        field.submitNumber(601)
        field.submitNumber(1.5)
        field.submitNumber("")
        compare(edits.count, 0)
        field.submitNumber(300)
        compare(edits.count, 1)
        compare(edits.signalArguments[0][1], 300)
    }
    function test_fraction_is_not_rounded_to_integer() {
        const field = findChild(options, "setting-field-strength")
        field.submitNumber("0.37")
        compare(edits.count, 1)
        fuzzyCompare(edits.signalArguments[0][1], 0.37, 0.000001)
    }
}
