import QtQuick
import QtTest
import "../../qml/components"
import "../../qml/style"

TestCase {
    name: "TranslatedDropdown"
    when: windowShown
    width: 640
    height: 480

    QtObject {
        id: dictionary
        property bool translated: false
        function tr(source) { return translated ? "translated: " + source : source }
    }
    StyledComboBox {
        id: selector
        width: 260
        translationContext: dictionary
        textRole: "label"
        valueRole: "code"
        model: {
            let choices = []
            for (let i = 0; i < 70; ++i)
                choices.push({label: "Option " + i, code: "id-" + i})
            return choices
        }
    }
    SignalSpy { id: modelSpy; target: selector; signalName: "modelChanged" }

    function initTestCase() { Theme.animations = false }
    function init() {
        dictionary.translated = false
        selector.currentIndex = 5
        modelSpy.clear()
    }
    function cleanup() { selector.popup.close() }

    function test_labels_do_not_change_stored_values() {
        compare(selector.currentText, "Option 5")
        compare(selector.currentValue, "id-5")
        dictionary.translated = true
        tryCompare(selector.contentItem, "text", "translated: Option 5")
        compare(selector.currentText, "Option 5")
        compare(selector.currentValue, "id-5")
        compare(modelSpy.count, 0)
        dictionary.translated = false
        tryCompare(selector.contentItem, "text", "Option 5")
    }

    function test_language_switch_preserves_open_scroll() {
        selector.popup.open()
        tryCompare(selector.popup, "opened", true)
        const list = selector.popup.contentItem
        tryVerify(function() { return list.contentHeight > list.height * 2 })
        wait(50)
        list.contentY = 300
        const before = list.contentY
        modelSpy.clear()
        dictionary.translated = true
        tryCompare(selector.contentItem, "text", "translated: Option 5")
        wait(50)
        compare(modelSpy.count, 0)
        compare(selector.currentIndex, 5)
        compare(selector.currentValue, "id-5")
        fuzzyCompare(list.contentY, before, 0.5)
        verify(selector.popup.visible)
    }
}
