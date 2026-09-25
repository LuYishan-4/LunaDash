import QtQuick
import QtQuick.Effects

Item {
    id: reveal
    property var sourceItem: null
    property real progress: 0
    readonly property bool prepared: sourceItem !== null
    Item {
        id: mask
        anchors.fill: parent
        layer.enabled: true
        visible: false
        Rectangle {
            readonly property real diameter: 2 * Math.hypot(reveal.width, reveal.height) * reveal.progress
            x: mask.width - width / 2
            y: mask.height - height / 2
            width: diameter
            height: diameter
            radius: diameter / 2
            color: "white"
        }
    }
    MultiEffect {
        anchors.fill: parent
        source: reveal.sourceItem
        autoPaddingEnabled: false
        maskEnabled: true
        maskSource: mask
        maskThresholdMin: 0.5
        maskSpreadAtMin: 0.015
    }
}
