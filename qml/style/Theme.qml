pragma Singleton
import QtQuick
QtObject {
    // Surface and text palette.
    readonly property color background: "#ee101418"
    readonly property color surface: "#de1c252e"
    readonly property color surfaceOpaque: "#f21c252e"
    readonly property color border: "#3b4856"
    property color accent: "#9ccbfb"
    readonly property color defaultAccent: "#9ccbfb"
    readonly property color lavender: "#d3bfe6"
    readonly property color text: "#e2e9f1"
    readonly property color muted: "#a5b4c4"
    readonly property color danger: "#f2b8b5"

    // Shared interactive-control surfaces. Buttons, fields, sliders and
    // switches read as one family instead of each hardcoding its own grey.
    readonly property color control: "#25313e"      // idle control fill
    readonly property color controlHover: "#344555" // hovered control fill
    readonly property color track: "#394656"        // inactive slider/switch track
    readonly property color knob: "#b6c3d3"         // inactive switch knob
    readonly property color accentInk: "#102133"    // content drawn on accent or bright fills
    readonly property color focusRing: "#e7edf5"    // light focus/selection ring

    property string font: "sans-serif"
    property bool clock24Hour: true
    property int barHeight: 40
    property bool animations: true
    property int animationDuration: 220
    readonly property int motion: animations ? animationDuration : 0
    readonly property int radius: 24
}
