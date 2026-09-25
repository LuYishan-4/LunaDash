import QtQuick
import QtTest
import "../../qml/wallpaper"

TestCase {
    id: test
    name: "WallpaperTransition"
    visible: true
    when: windowShown
    width: 200
    height: 120
    readonly property url red: Qt.resolvedUrl("fixtures/red.svg")
    readonly property url blue: Qt.resolvedUrl("fixtures/blue.svg")
    readonly property url green: Qt.resolvedUrl("fixtures/green.svg")
    Component { id: component; WallpaperTransition { width: 200; height: 120; animationsEnabled: true; duration: 900 } }
    function create() {
        const item = createTemporaryObject(component, test, {source: red})
        verify(item !== null)
        tryCompare(item, "displayedSource", red)
        tryCompare(item, "ready", true)
        return item
    }
    function test_reveals_from_bottom_right_and_retains_old_image() {
        const item = create()
        item.source = blue
        tryCompare(item, "transitioning", true)
        compare(item.displayedSource, red)
        tryVerify(() => item.progress >= 0.28 && item.progress < 0.7)
        waitForRendering(item)
        const image = grabImage(item)
        verify(image.red(8, 8) > 240)
        verify(image.blue(190, 110) > 240)
        tryCompare(item, "transitioning", false)
        compare(item.displayedSource, blue)
        waitForRendering(item)
        verify(grabImage(item).blue(8, 8) > 240)
    }
    function test_latest_selection_wins_and_invalid_images_keep_old_wallpaper() {
        const item = create()
        item.source = blue
        tryCompare(item, "transitioning", true)
        item.source = green
        tryCompare(item, "displayedSource", green, 3500)
        tryCompare(item, "transitioning", false)
        item.source = Qt.resolvedUrl("fixtures/missing-image.png")
        tryCompare(item, "failed", true)
        compare(item.displayedSource, green)
    }
    function test_disabling_motion_still_waits_for_a_ready_image() {
        const item = create()
        item.animationsEnabled = false
        item.source = blue
        tryCompare(item, "displayedSource", blue)
        verify(!item.transitioning)
        verify(item.ready)
    }
}
