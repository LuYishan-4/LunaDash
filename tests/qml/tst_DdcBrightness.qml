import QtQuick
import QtTest
import "../../qml/settings/components"
import "../../qml/style"

Item {
    width: 500; height: 700
    QtObject {
        id: mockShell
        property var state: ({})
        property var requests: []
        function tr(text) { return text }
        function command(action, value) { requests.push({action: action, value: JSON.parse(value)}) }
    }
    Component { id: factory; DdcBrightnessControls { width: 300; shell: mockShell } }
    TestCase {
        name: "DdcBrightness"
        when: windowShown
        function state(percent) {
            return {ddcBrightness: {installed:true, scanning:false, devices:[
                {id:"i2c-7:first", label:"First external monitor with a long descriptive name", available:true, percent:percent},
                {id:"i2c-9:second", label:"Second monitor", available:true, percent:75},
                {id:"i2c-8:unsupported", label:"Unsupported monitor", available:false, percent:0}
            ]}}
        }
        function sliders(item, result) {
            if (item.objectName === "ddcBrightnessSlider") result.push(item)
            for (let index = 0; index < item.children.length; ++index)
                sliders(item.children[index], result)
        }
        function test_target_and_polling() {
            Theme.animations = false
            mockShell.state = state(40)
            mockShell.requests = []
            const control = createTemporaryObject(factory, parent)
            verify(control !== null)
            wait(20)
            const initial = []
            sliders(control, initial)
            compare(initial.length, 3)
            compare(initial[0].value, 40)
            compare(initial[1].value, 75)
            compare(initial[2].enabled, false)
            initial[1].value = 55
            initial[1].moved()
            mockShell.state = state(42)
            wait(250)
            const updated = []
            sliders(control, updated)
            compare(updated[1], initial[1], "Status polling preserves active delegates")
            compare(mockShell.requests.length, 1)
            compare(mockShell.requests[0].action, "ddc-brightness")
            compare(mockShell.requests[0].value.id, "i2c-9:second")
            compare(mockShell.requests[0].value.percent, 55)
            updated[0].value = 60
            updated[0].moved()
            const replaced = state(20)
            replaced.ddcBrightness.devices[0].id = "i2c-7:replacement"
            mockShell.state = replaced
            wait(250)
            compare(mockShell.requests.length, 1, "Never apply a queued change to a replacement monitor")
        }
    }
}
