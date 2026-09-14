import QtQuick
import QtQuick.Shapes
import "../style"

Item {
    id: logo
    property bool animated: true
    property color primaryColor: Theme.accent
    property color secondaryColor: Theme.lavender
    property color inkColor: Theme.text
    property real progress: animated ? 0 : 1
    implicitWidth: 120
    implicitHeight: 120

    function restart() {
        reveal.stop()
        progress = animated ? 0 : 1
        if (animated) reveal.start()
    }

    Item {
        id: artwork
        anchors.fill: parent
        opacity: logo.progress
        scale: 0.72 + logo.progress * 0.28
        rotation: -22 * (1 - logo.progress)

        Shape {
            anchors.fill: parent
            ShapePath {
                strokeColor: Qt.rgba(logo.primaryColor.r, logo.primaryColor.g, logo.primaryColor.b, 0.45)
                strokeWidth: 2
                fillColor: "transparent"
                PathSvg { path: "M18 72 C24 103 58 119 88 104 C111 93 121 67 112 43" }
            }
            ShapePath {
                strokeColor: logo.primaryColor
                strokeWidth: 1.5
                fillColor: logo.primaryColor
                PathSvg { path: "M65 11 C38 17 23 43 31 69 C39 96 70 110 95 97 C77 96 60 85 53 68 C44 47 50 25 65 11 Z" }
            }
            ShapePath {
                strokeColor: "transparent"
                fillColor: logo.secondaryColor
                PathSvg { path: "M92 19 L95 27 L103 30 L95 33 L92 41 L89 33 L81 30 L89 27 Z" }
            }
            ShapePath {
                strokeColor: "transparent"
                fillColor: logo.inkColor
                PathSvg { path: "M105 54 L107 59 L112 61 L107 63 L105 68 L103 63 L98 61 L103 59 Z" }
            }
            ShapePath {
                strokeColor: "transparent"
                fillColor: logo.primaryColor
                PathSvg { path: "M78 8 L80 12 L84 14 L80 16 L78 20 L76 16 L72 14 L76 12 Z" }
            }
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
