import QtQuick
import QtQuick.Window
import Quickshell
import "../style"

Item {
    id: logo
    property bool animated: true
    property color primaryColor: Theme.accent
    property color secondaryColor: Theme.lavender
    property color inkColor: Theme.text
    property real progress: animated ? 0 : 1
    // The software shell backend can leave stale pixels behind when an animated
    // item changes scale or rotation, because its damage tracking misses the old
    // transformed bounds. The compatibility path therefore reveals the logo with
    // opacity only; the GPU path keeps the moon's tilt-and-zoom entrance.
    readonly property bool softwareRenderer: Quickshell.env("LUDASH_SHELL_RENDERER") === "software"
    implicitWidth: 120
    implicitHeight: 120

    function restart() {
        reveal.stop()
        progress = animated ? 0 : 1
        if (animated) reveal.start()
    }

    function hexColor(value) {
        const part = component => Math.max(0, Math.min(255, Math.round(component * 255))).toString(16).padStart(2, "0")
        return "#" + part(value.r) + part(value.g) + part(value.b)
    }

    // Qt Quick Shapes leaves stale pixels behind on the software shell backend
    // whenever a shape moves. The artwork is therefore rasterized from the same
    // paths through QtSvg, which has well-defined bounds.
    readonly property string svgData: "data:image/svg+xml;utf8," + encodeURIComponent(
        "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 126 126' width='126' height='126'>" +
        "<path d='M18 72 C24 103 58 119 88 104 C111 93 121 67 112 43' fill='none' stroke='" + logo.hexColor(logo.primaryColor) + "' stroke-opacity='0.45' stroke-width='2'/>" +
        "<path d='M65 11 C38 17 23 43 31 69 C39 96 70 110 95 97 C77 96 60 85 53 68 C44 47 50 25 65 11 Z' fill='" + logo.hexColor(logo.primaryColor) + "' stroke='" + logo.hexColor(logo.primaryColor) + "' stroke-width='1.5'/>" +
        "<path d='M92 19 L95 27 L103 30 L95 33 L92 41 L89 33 L81 30 L89 27 Z' fill='" + logo.hexColor(logo.secondaryColor) + "'/>" +
        "<path d='M105 54 L107 59 L112 61 L107 63 L105 68 L103 63 L98 61 L103 59 Z' fill='" + logo.hexColor(logo.inkColor) + "'/>" +
        "<path d='M78 8 L80 12 L84 14 L80 16 L78 20 L76 16 L72 14 L76 12 Z' fill='" + logo.hexColor(logo.primaryColor) + "'/></svg>")

    Item {
        id: artwork
        anchors.fill: parent
        opacity: logo.progress
        scale: logo.softwareRenderer ? 1 : 0.72 + logo.progress * 0.28
        rotation: logo.softwareRenderer ? 0 : -22 * (1 - logo.progress)

        Image {
            anchors.fill: parent
            source: logo.svgData
            sourceSize.width: Math.max(1, Math.round(width * Screen.devicePixelRatio))
            sourceSize.height: Math.max(1, Math.round(height * Screen.devicePixelRatio))
            fillMode: Image.PreserveAspectFit
            smooth: true
        }
    }

    NumberAnimation {
        id: reveal
        target: logo
        property: "progress"
        from: 0
        to: 1
        duration: Theme.animations ? Math.max(320, Theme.animationDuration * 3) : 0
        easing.type: Easing.OutBack
    }

    Component.onCompleted: restart()
    onAnimatedChanged: restart()
}
