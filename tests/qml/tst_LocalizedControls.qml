import QtQuick
import QtQuick.Layouts
import QtTest
import "../../qml/components"
import "../../qml/style"

Item {
    width: 480; height: 800
    Component { id: buttonFactory; ShellButton {} }
    Component { id: switchFactory; SoftSwitch {} }
    Component { id: segmentFactory; Segment {} }
    Component { id: comboFactory; StyledComboBox {} }
    Component {
        id: rowFactory
        RowLayout {
            property string label: ""
            width: 280
            ShellButton { Layout.fillWidth:true; text:parent.label }
            ShellButton { Layout.fillWidth:true; text:parent.label }
        }
    }
    TestCase {
        name: "LocalizedControls"
        when: windowShown
        function initTestCase() { Theme.animations = false }
        function dictionary(locale) {
            if (locale === "en_US") return ({})
            const request = new XMLHttpRequest()
            request.open("GET", Qt.resolvedUrl("../../data/translations/" + locale + (locale === "zh_TW" ? ".json" : "/complete.json")), false)
            request.send()
            return JSON.parse(request.responseText)
        }
        function test_controls_data() {
            const result = []
            for (const locale of ["en_US", "zh_TW", "zh_CN", "ja_JP"]) {
                const map = dictionary(locale)
                const source = "Restore shortcut defaults"
                result.push({tag:locale, label:map[source] || source})
            }
            result.push({tag:"long-label", label:"A translated option with several long words that must remain inside the control"})
            return result
        }
        function checkText(item) {
            if (item.paintedWidth !== undefined && item.visible) {
                verify(item.paintedWidth <= item.width + 1, "Text exceeds width: " + item.text)
                verify(item.paintedHeight <= item.height + 1, "Text exceeds height: " + item.text)
            }
            if (item.children)
                for (const child of item.children) checkText(child)
        }
        function test_controls(data) {
            for (const factory of [buttonFactory, switchFactory, segmentFactory]) {
                const control = createTemporaryObject(factory, parent, {width:160, text:data.label})
                verify(control !== null)
                wait(20)
                checkText(control)
            }
            const combo = createTemporaryObject(comboFactory, parent, {width:160, model:[data.label]})
            verify(combo !== null)
            wait(20)
            checkText(combo)
            const row = createTemporaryObject(rowFactory, parent, {label:data.label})
            verify(row !== null)
            wait(20)
            for (const control of row.children) {
                verify(control.x + control.width <= row.width + 1, "Button exceeds its row")
                checkText(control)
            }
        }
    }
}
