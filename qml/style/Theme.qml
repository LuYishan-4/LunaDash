pragma Singleton
import QtQuick
QtObject {
    readonly property color background: "#ee0b1020"
    readonly property color surface: "#de141c31"
    readonly property color surfaceOpaque: "#f2162036"
    readonly property color border: "#465777"
    property color accent: "#9ccbfb"
    property color secondaryAccent: "#6e5f9f"
    readonly property color defaultAccent: "#9ccbfb"
    readonly property color defaultSecondaryAccent: "#6e5f9f"
    readonly property color moon: "#dbe9ff"
    readonly property color starlight: "#b7ccff"
    readonly property color lavender: "#cdbdf3"
    readonly property color text: "#edf3ff"
    readonly property color muted: "#aab7d1"
    readonly property color danger: "#f2b8c6"

    readonly property color control: "#202b45"
    readonly property color controlHover: "#2c3b5c"
    readonly property color track: "#33415f"
    readonly property color knob: "#d8e4fb"
    readonly property color accentInk: "#10182a"
    readonly property color focusRing: "#e8f0ff"

    property string font: "sans-serif"
    property bool clock24Hour: true
    property int barHeight: 40
    property bool animations: true
    property int animationDuration: 220
    readonly property int motion: animations ? animationDuration : 0
    readonly property int radius: 24
    readonly property int radiusLarge: 28
}
