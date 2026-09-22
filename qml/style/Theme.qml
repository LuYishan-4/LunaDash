pragma Singleton
import QtQuick

QtObject {
    readonly property color background: "#ee0b1020"
    readonly property color surface: "#de141c31"
    readonly property color surfaceOpaque: "#f2162036"
    readonly property color surfaceElevated: "#f51a2440"
    readonly property color surfaceHover: "#f0222e4c"
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
    readonly property color success: "#a8e6cf"
    readonly property color warning: "#f4d58d"

    readonly property color control: "#202b45"
    readonly property color controlHover: "#2c3b5c"
    readonly property color track: "#33415f"
    readonly property color knob: "#d8e4fb"
    readonly property color accentInk: "#10182a"
    readonly property color focusRing: "#e8f0ff"
    readonly property color scrim: "#99060a14"

    property string font: "sans-serif"
    property bool clock24Hour: true
    property int barHeight: 40
    property string panelEdge: "top"
    property int panelExtent: 40
    readonly property int panelTopInset: panelEdge === "top" ? panelExtent : 0
    readonly property int panelBottomInset: panelEdge === "bottom" ? panelExtent : 0
    readonly property int panelLeftInset: panelEdge === "left" ? panelExtent : 0
    readonly property int panelRightInset: panelEdge === "right" ? panelExtent : 0
    property bool animations: true
    property int animationDuration: 220

    readonly property int motion: animations ? animationDuration : 0
    readonly property int motionFast: animations ? Math.max(80, Math.round(animationDuration * 0.55)) : 0
    readonly property int motionSlow: animations ? Math.max(260, Math.round(animationDuration * 1.45)) : 0

    readonly property int spacingXs: 4
    readonly property int spacingSm: 8
    readonly property int spacingMd: 12
    readonly property int spacingLg: 18
    readonly property int spacingXl: 24

    readonly property int radiusSmall: 10
    readonly property int radiusMedium: 16
    readonly property int radius: 24
    readonly property int radiusLarge: 28
}
