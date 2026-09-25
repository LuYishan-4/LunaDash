import QtQuick
import QtTest

TestCase {
    id: test
    name: "InteractionTemplates"
    when: windowShown
    visible: true
    width: 640; height: 400
    property var sent: []
    function command(method, value) { sent = sent.concat([[method, String(value)]]) }
    Loader { id: loader; anchors.fill: parent }
    function cleanup() { loader.source = ""; sent = [] }
    function templateContext(progress) {
        return {progress: progress, background: "#182030", accent: "#aabbff",
            foreground: "#ffffff", muted: "#999999", fontFamily: "sans-serif",
            interaction: {scope: "windows", index: 0, windows: [
                {id: 31, title: "Editor", appId: "editor"},
                {id: 32, title: "Terminal", appId: "terminal"}]}}
    }
    function test_transitionReachesFullCoverageAndReleases() {
        loader.setSource(Qt.resolvedUrl("../../templates/plugins/plugin-transition/Main.qml"),
            {shell: test, settings: {opacity: 1}, context: templateContext(0.5)})
        tryCompare(loader, "status", Loader.Ready)
        waitForRendering(loader)
        compare(grabImage(loader).pixel(20, 20), "#182030")
        loader.item.context = templateContext(1)
        waitForRendering(loader)
        compare(loader.item.cover, 0)
    }
    function test_windowTemplateConfirmsAnIdNotAWorkspace() {
        loader.setSource(Qt.resolvedUrl("../../templates/plugins/window-switcher/Main.qml"),
            {shell: test, settings: {opacity: 1}, context: templateContext(0)})
        tryCompare(loader, "status", Loader.Ready)
        tryVerify(() => findChild(loader.item, "preview-card-31") !== null)
        const card = findChild(loader.item, "preview-card-31")
        mouseClick(card, card.width / 2, card.height / 2)
        verify(sent.some(entry => entry[0] === "switch-window" && entry[1] === "31"))
        verify(sent.some(entry => entry[0] === "switch-accept"))
        verify(!sent.some(entry => entry[0] === "workspace"))
    }
}
