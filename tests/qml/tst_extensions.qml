import QtQuick
import QtTest
import "../../qml/plugins"

TestCase {
    name: "ExtensionSlots"
    when: windowShown
    visible: true
    width: 320
    height: 180
    QtObject {
        id: backend
        property var state: ({extensions: {installed: [], targets: []}})
        property string error: ""
        function command(method, value) { error = value; }
    }
    ExtensionSlot {
        id: slot
        anchors.fill: parent
        shell: backend
        target: "panel"
        Rectangle { anchors.fill: parent; color: "red" }
    }
    function descriptor(mode, entry) {
        return {id: "test.plugin", schemaVersion: 2, type: "quickshell", target: "panel", mode: mode, enabled: true, available: true,
            settings: {color: "blue"}, entry: entry || Qt.resolvedUrl("fixtures/plugins/Replacement.qml").toString()};
    }
    function apply(plugins) { backend.state = {extensions: {installed: plugins, targets: []}}; }
    function cleanup() { slot.forceBuiltin = false; apply([]); backend.error = ""; }
    function test_replaceAndDisable() {
        apply([descriptor("replace")]);
        tryCompare(slot, "replacementReady", true);
        verify(!slot.builtinVisible);
        compare(grabImage(slot).pixel(50, 50), "#0000ff");
        apply([]);
        tryCompare(slot, "replacementReady", false);
        verify(slot.builtinVisible);
        compare(grabImage(slot).pixel(50, 50), "#ff0000");
    }
    function test_waitsUntilReplacementIsReady() {
        const plugin = descriptor("replace");
        plugin.settings.ready = false;
        apply([plugin]);
        tryVerify(() => findChild(slot, "testPlugin") !== null);
        verify(slot.builtinVisible);
        compare(grabImage(slot).pixel(50, 50), "#ff0000");
        findChild(slot, "testPlugin").pluginReady = true;
        tryCompare(slot, "replacementReady", true);
        compare(grabImage(slot).pixel(50, 50), "#0000ff");
    }
    function test_augmentAndStableInstance() {
        apply([descriptor("augment")]);
        tryVerify(() => findChild(slot, "testPlugin") !== null);
        verify(slot.builtinVisible);
        const instance = findChild(slot, "testPlugin");
        apply([descriptor("augment")]);
        wait(30);
        compare(findChild(slot, "testPlugin"), instance);
    }
    function test_recoveryBypass() {
        apply([descriptor("replace")]);
        tryCompare(slot, "replacementReady", true);
        slot.forceBuiltin = true;
        tryCompare(slot, "replacementReady", false);
        verify(slot.builtinVisible);
    }
    function test_failedReplacementKeepsBuiltin() {
        apply([descriptor("replace", Qt.resolvedUrl("fixtures/plugins/Missing.qml").toString())]);
        tryVerify(() => backend.error.length > 0);
        verify(slot.builtinVisible);
        verify(!slot.replacementReady);
    }
}
