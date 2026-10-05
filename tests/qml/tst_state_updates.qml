import QtQuick
import QtTest
import "../../qml/settings"
import "../../qml/session/StateUpdates.js" as Updates

TestCase {
    id: test
    name: "StateUpdates"
    width: 640
    height: 480
    when: windowShown
    visible: true
    QtObject {
        id: shellMock
        property var state: ({system: {batteryPercent: 10}, power: {}})
        property var systemState: ({batteryPercent: 30})
        function tr(text) { return text }
    }
    Component {
        id: powerView
        SettingsPageView { shell: shellMock; category: "power" }
    }
    function test_batteryUpdatesWithoutReplacingDesktopState() {
        const page = createTemporaryObject(powerView, test, {width: 600, height: 440})
        verify(page)
        tryCompare(page, "ready", true)
        waitForRendering(page)
        const label = findChild(page, "batteryLevel")
        verify(label)
        compare(label.text, "Battery  30%")
        shellMock.systemState = {batteryPercent: 42}
        compare(label.text, "Battery  42%")
        compare(shellMock.state.system.batteryPercent, 10)
    }
    function test_idleTelemetryDoesNotRebindDesktop() {
        const previous = {language: "zh_TW", translations: {Welcome: "test"}, system: {cpu: 2},
            display: {width: 800, frameCallbacks: 10, eventLoop: {dispatchCalls: 5}}}
        const incoming = {language: "zh_TW", system: {cpu: 3},
            display: {width: 1280, frameCallbacks: 20, eventLoop: {dispatchCalls: 9}, error: "mode rejected"}}
        const result = Updates.prepare(incoming, previous, false)
        compare(result.translations, previous.translations)
        compare(result.system, previous.system)
        compare(result.display.frameCallbacks, 10)
        compare(result.display.width, 1280)
        compare(result.display.error, "mode rejected")
        compare(previous.display.width, 800)
    }
    function test_liveMetricsAndLanguageSwitch() {
        const previous = {language: "en_US", translations: {Welcome: "old"}, display: {frameCallbacks: 1}}
        const incoming = {language: "ja_JP", translations: {Welcome: "new"}, display: {frameCallbacks: 2}}
        const result = Updates.prepare(incoming, previous, true)
        compare(result.translations.Welcome, "new")
        compare(result.display.frameCallbacks, 2)
        compare(Updates.prepare({language: "en_US"}, {}, false).translations, undefined)
    }
}
