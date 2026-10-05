import QtQuick
import QtTest
import "../../qml/welcome"

TestCase {
    id: test
    name: "Welcome"
    width: 900
    height: 720
    when: windowShown
    QtObject {
        id: shellMock
        property var calls: []
        property var state: ({language: "en_US", shortcuts: {launchTerminal: "Meta+T"}, appearance: {animations: true}})
        function tr(text) { return text }
        function command(method, value) { calls.push([method, value]) }
    }
    Component {
        id: content
        WelcomeContent { shell: shellMock }
    }
    function init() { shellMock.calls = [] }
    function test_sizes_data() {
        return [{tag: "narrow", w: 360, h: 480}, {tag: "default", w: 800, h: 660}, {tag: "wide", w: 1920, h: 1080}]
    }
    function test_sizes(data) {
        const view = createTemporaryObject(content, test, {width: data.w, height: data.h})
        verify(view)
        waitForRendering(view)
        const finish = findChild(view, "welcomeFinish")
        const point = finish.mapToItem(view, 0, 0)
        verify(point.x >= 0 && point.y >= 0)
        verify(point.x + finish.width <= view.width)
        verify(point.y + finish.height <= view.height)
    }
    function test_keyboardAndActions() {
        const view = createTemporaryObject(content, test, {width: 800, height: 660})
        verify(view)
        const motion = findChild(view, "welcomeReducedMotion")
        motion.forceActiveFocus()
        keyClick(Qt.Key_Space)
        compare(shellMock.calls.length, 1)
        compare(shellMock.calls[0][0], "appearance")
        compare(JSON.parse(shellMock.calls[0][1]).animations, false)
        const finish = findChild(view, "welcomeFinish")
        const spy = createTemporaryObject(signalSpy, test, {target: view, signalName: "finished"})
        finish.forceActiveFocus()
        keyClick(Qt.Key_Return)
        compare(spy.count, 1)
    }
    Component { id: signalSpy; SignalSpy {} }
}
