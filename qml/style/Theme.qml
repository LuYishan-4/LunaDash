pragma Singleton
import QtQuick
QtObject {
    readonly property color background: "#ee101418"
    readonly property color surface: "#de1c252e"
    readonly property color border: "#3b4856"
    property color accent: "#9ccbfb"
    readonly property color lavender: "#d3bfe6"
    readonly property color text: "#e2e9f1"
    readonly property color muted: "#a5b4c4"
    readonly property color danger: "#f2b8b5"
    readonly property string font: "sans-serif"
    property int barHeight: 40
    property bool animations: true
    property int animationDuration: 220
    readonly property int motion: animations ? animationDuration : 0
    readonly property int radius: 24
}
