import QtQuick
import QtTest
import "../../qml/wallpaper/GalleryModel.js" as Gallery
import "../../qml/components"
import "../../qml/style"

TestCase {
    id: test
    name: "WallpaperGalleryPolicy"
    when: windowShown
    visible: true
    width: 500
    height: 320
    readonly property var entries: [
        {path:"/pictures/walls/night sky.png", name:"Night sky", category:"Nature", type:"image", favorite:true},
        {path:"/pictures/walls/video/rain.mp4", name:"Rain", category:"video", type:"video"},
        {path:"/pictures/walls-other/hidden.png", name:"Not here", category:"Other", type:"image"},
        {path:"/usr/share/walls/default.png", name:"Default", category:"Bundled", type:"image", bundled:true},
        {path:"/pictures/walls/night sky.png", name:"duplicate", category:"Nature", type:"image"}
    ]
    QtObject {
        id: backend
        property var state: ({appearance: {blur:true}, blurReady:true, blurFailed:false})
    }
    Component { id: surfaceComponent; GlassSurface { shell: backend; width: 100; height: 100 } }
    function test_directory_is_bounded_and_duplicates_removed() {
        const result = Gallery.visibleEntries(entries, "/pictures/walls/", "library", "", "all", "")
        compare(result.length, 2)
        compare(result[0].name, "Night sky")
        compare(result[1].type, "video")
        compare(Gallery.visibleEntries(entries, "", "library", "", "all", "").length, 0)
    }
    function test_filters_and_case_insensitive_search() {
        compare(Gallery.visibleEntries(entries, "/pictures/walls", "library", "video", "video", "RAIN").length, 1)
        compare(Gallery.visibleEntries(entries, "/pictures/walls", "library", "video", "image", "").length, 0)
        compare(Gallery.visibleEntries(entries, "/pictures/walls", "favorites", "", "all", "sky").length, 1)
        compare(Gallery.visibleEntries(entries, "/pictures/walls", "bundled", "", "all", "")[0].name, "Default")
    }
    function test_keyboard_navigation_never_leaves_model() {
        compare(Gallery.nextIndex(-1, 1, 0), -1)
        compare(Gallery.nextIndex(0, -4, 6), 0)
        compare(Gallery.nextIndex(5, 4, 6), 5)
        compare(Gallery.nextIndex(1, 4, 6), 5)
    }
    function test_glass_falls_back_to_opaque_without_real_backdrop() {
        const previous = Theme.animations
        Theme.animations = false
        const panel = createTemporaryObject(surfaceComponent, test)
        verify(panel !== null)
        verify(panel.glassAvailable)
        verify(panel.color.a < 1)
        backend.state = {appearance: {blur:true}, blurReady:false}
        compare(panel.glassAvailable, false)
        compare(panel.color.a, 1)
        backend.state = {appearance: {blur:true, eyeCare:true}, blurReady:true}
        compare(panel.glassAvailable, false)
        compare(panel.color.a, 1)
        backend.state = {appearance: {blur:true}, blurReady:true, blurFailed:true}
        compare(panel.glassAvailable, false)
        compare(panel.color.a, 1)
        backend.state = {appearance: {blur:true}, blurReady:true, blurFailed:false}
        Theme.animations = previous
    }
}
