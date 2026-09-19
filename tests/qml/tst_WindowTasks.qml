import QtQuick
import QtTest
import "../../qml/columns"
import "../../qml/style"

Item {
    width: 400; height: 100
    QtObject {
        id: mockShell
        property var calls: []
        property var dropTarget: 0
        function tr(text) { return text }
        function command(action, value) { calls.push({action: action, value: value}) }
    }
    Component { id: factory; ColumnCell { width: 100; height: 40; shell: mockShell } }
    TestCase {
        name: "WindowTasks"
        when: windowShown
        function member(id, focused) {
            return {window:id, appId:"lunadash-app", icon:"lunadash", title:"Window " + id, focused:focused, minimized:false}
        }
        function group(first, second) {
            return {focused:true, members:[member(first, false), member(second, true)]}
        }
        function buttons(item, result) {
            if (item.objectName === "windowTaskButton") result.push(item)
            for (let i = 0; i < item.children.length; ++i) buttons(item.children[i], result)
        }
        function init() { Theme.animations = false; mockShell.calls = [] }
        function test_padding_and_group_focus() {
            const cell = createTemporaryObject(factory, parent, {group:group(11, 22)})
            verify(cell !== null)
            const items = []; buttons(cell, items)
            compare(items.length, 2)
            mouseClick(items[1], 1, 28)
            compare(mockShell.calls.length, 1)
            compare(mockShell.calls[0].value, 22, "Padding belongs to the visible member")
            mouseClick(cell, 1, 20)
            compare(mockShell.calls[1].value, 22, "Group background selects its focused member")
        }
        function test_polling_and_replacement() {
            const cell = createTemporaryObject(factory, parent, {group:group(11, 22)})
            const before = []; buttons(cell, before)
            mousePress(before[1], 15, 15)
            cell.group = group(11, 22)
            wait(30)
            const after = []; buttons(cell, after)
            compare(after[1], before[1], "Polling preserves the mouse grab")
            mouseRelease(after[1], 15, 15)
            compare(mockShell.calls.length, 1)
            compare(mockShell.calls[0].value, 22)
            mousePress(after[1], 15, 15)
            cell.group = group(22, 33)
            wait(30)
            mouseRelease(after[1], 15, 15)
            compare(mockShell.calls.length, 1, "A replacement must not receive the old click")
        }
        function test_drag_and_right_click() {
            const cell = createTemporaryObject(factory, parent, {group:group(11, 22)})
            const items = []; buttons(cell, items)
            mouseClick(items[1], 15, 15, Qt.RightButton)
            compare(mockShell.calls[0].action, "expel-window")
            compare(mockShell.calls[0].value, 22)
            mockShell.calls = []
            mousePress(items[0], 15, 15)
            mouseMove(items[0], 23, 15, 20)
            mouseMove(items[0], 35, 15, 20)
            mouseMove(items[0], 49, 15, 20)
            compare(items[0].drag.active, true, "Drag started")
            compare(items[0].wasDragged, true, "Drag tracked")
            tryCompare(mockShell, "dropTarget", 22, 500, "Drop target selected")
            mouseRelease(items[0], 49, 15)
            compare(mockShell.calls.length, 1)
            compare(mockShell.calls[0].action, "group-window")
            const payload = JSON.parse(mockShell.calls[0].value)
            compare(payload.window, 11)
            compare(payload.target, 22)
        }
    }
}
