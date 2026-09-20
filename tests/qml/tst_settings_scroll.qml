import QtQuick
import QtQuick.Controls
import QtTest
import "../../qml/settings"

TestCase {
    id: test
    name: "SettingsScrolling"
    when: windowShown
    visible: true
    width: 820
    height: 480

    QtObject {
        id: backend
        property real fixtureHeight: 1800
        property bool footerClicked: false
        property var state: ({extensions: {installed: [], targets: [], document: {schemaVersion: 1, builtins: {}, plugins: {}}}})
        signal commandCompleted(string method, var result)
        function tr(text) { return text; }
        function command(method, value) {}
    }
    Component {
        id: viewComponent
        SettingsPageView { width: 760; height: 420; shell: backend }
    }
    function fixture() {
        const view = createTemporaryObject(viewComponent, test, {pageSource: Qt.resolvedUrl("fixtures/SettingsPage.qml")});
        verify(view !== null);
        tryCompare(view, "ready", true);
        waitForRendering(view);
        wait(300);
        return view;
    }
    function init() {
        backend.fixtureHeight = 1800;
        backend.footerClicked = false;
        backend.state = {extensions: {installed: [], targets: [], document: {schemaVersion: 1, builtins: {}, plugins: {}}}};
    }
    function test_mouseWheelReachesLastSetting() {
        const view = fixture();
        tryCompare(view, "contentHeight", 1800);
        verify(view.ScrollBar.vertical.size < 1);
        mouseWheel(view, 100, 150, 0, -120);
        tryVerify(() => view.contentItem.contentY > 0);
        for (let i = 0; i < 40 && !view.contentItem.atYEnd; ++i) {
            mouseWheel(view, 100, 150, 0, -240);
            wait(60);
        }
        tryVerify(() => view.contentItem.atYEnd);
        tryCompare(view.contentItem, "moving", false);
        const footer = findChild(view, "pageFooter");
        waitForRendering(footer);
        mouseClick(footer, footer.width / 2, footer.height / 2);
        tryCompare(backend, "footerClicked", true);
    }
    function test_scrollbarAndDynamicHeight() {
        const view = fixture();
        const bar = view.ScrollBar.vertical;
        mouseDrag(bar, bar.width / 2, 10, 0, bar.height - 25);
        tryVerify(() => view.contentItem.contentY > 0);
        backend.fixtureHeight = 2300;
        tryCompare(view, "contentHeight", 2300);
        backend.fixtureHeight = 100;
        tryCompare(view, "contentHeight", 100);
        tryVerify(() => view.contentItem.contentY <= 0);
    }
    function test_switchToIntegratedPluginsResetsScroll() {
        const view = fixture();
        mouseWheel(view, 100, 150, 0, -600);
        tryVerify(() => view.contentItem.contentY > 0);
        view.category = "plugins";
        view.pageSource = Qt.resolvedUrl("../../qml/settings/pages/plugins.qml");
        tryCompare(view, "ready", true);
        tryCompare(view.contentItem, "contentY", 0);
        verify(findChild(view, "extensionJsonEditor") !== null);
        const advanced = findButton(view, "Advanced JSON");
        verify(advanced !== null);
        const before = view.contentHeight;
        // Activate the real settings control after scrolling it into view.
        view.contentItem.contentY = Math.max(0, view.contentHeight - view.height);
        mouseClick(advanced);
        tryVerify(() => view.contentHeight > before + 200);
        verify(view.ScrollBar.vertical.size < 1);
    }
    function test_replacementPageScrollsAndRemovalRestoresBuiltin() {
        const descriptor = {id: "test.settings", schemaVersion: 2, type: "quickshell", target: "settings.general", mode: "replace", enabled: true, available: true,
            settings: {color: "blue", pageHeight: 1400}, entry: Qt.resolvedUrl("fixtures/plugins/Replacement.qml").toString()};
        backend.state = {extensions: {installed: [descriptor], targets: []}};
        const view = fixture();
        tryCompare(view, "contentHeight", 1400);
        mouseWheel(view, 100, 150, 0, -120);
        tryVerify(() => view.contentItem.contentY > 0);
        backend.state = {extensions: {installed: [], targets: []}};
        tryCompare(view, "contentHeight", 1800);
    }
    function findButton(item, text) {
        if (item.text === text && item.clicked !== undefined)
            return item;
        for (const child of item.children || []) {
            const match = findButton(child, text);
            if (match)
                return match;
        }
        return null;
    }
}
