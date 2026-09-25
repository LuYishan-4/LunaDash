pragma Singleton
import QtQuick

QtObject {
    property var palette: ({})
    property bool eyeCare: false
    readonly property bool dark: palette.dark ?? true
    function tone(name, fallback) { return palette[name] || fallback }
    function translucent(color, alpha) {
        return Qt.rgba(color.r, color.g, color.b, eyeCare ? 1 : alpha)
    }
    readonly property color base: tone("background", "#111318")
    readonly property color background: translucent(base, 0.96)
    readonly property color surface: tone("surface", "#191c22")
    readonly property color surfaceOpaque: surface
    readonly property color surfaceElevated: tone("surfaceElevated", "#22262e")
    readonly property color surfaceHover: tone("surfaceHover", "#303640")
    readonly property color surfaceGlass: translucent(surface, 0.86)
    readonly property color surfaceStrong: translucent(base, 0.98)
    readonly property color border: tone("border", "#586172")
    readonly property color hairline: tone("hairline", "#353b46")

    property color accent: tone("accent", defaultAccent)
    property color secondaryAccent: tone("secondaryAccent", "#bbc7d9")
    readonly property color defaultAccent: "#9ccbfb"
    readonly property color defaultSecondaryAccent: "#bbc7d9"
    readonly property color moon: text
    readonly property color starlight: secondaryAccent
    readonly property color lavender: secondaryAccent
    readonly property color text: tone("text", "#e2e5ed")
    readonly property color muted: tone("muted", "#bdc5d3")
    readonly property color danger: tone("danger", "#ffb4ab")
    readonly property color success: tone("success", "#b4d7ad")
    readonly property color warning: tone("warning", "#efcd8c")

    readonly property color control: surfaceElevated
    readonly property color controlHover: surfaceHover
    readonly property color track: hairline
    readonly property color knob: accent
    readonly property color accentInk: tone("accentInk", "#152331")
    readonly property color focusRing: accent
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
    readonly property int radiusLarge: 30
    readonly property int radiusHero: 36
}
