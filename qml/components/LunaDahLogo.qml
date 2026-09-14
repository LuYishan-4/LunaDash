import QtQuick
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
        if (animated)
            reveal.start()
    }

    Rectangle {
        id: orbit
        anchors.centerIn: parent
        width: Math.min(parent.width, parent.height) * 0.76
        height: width
        radius: width / 2
        color: "transparent"
        border.width: Math.max(2, width * 0.035)
        border.color: logo.primaryColor
        opacity: 0.22 + 0.78 * logo.progress
        scale: 0.72 + 0.28 * logo.progress
        rotation: -38 + 38 * logo.progress
    }

    Rectangle {
        anchors.centerIn: parent
        width: orbit.width * 0.58
        height: width
        radius: width * 0.16
        rotation: 45 + 90 * (1 - logo.progress)
        color: logo.secondaryColor
        opacity: 0.16 + 0.84 * logo.progress
        scale: 0.55 + 0.45 * logo.progress
    }

    Rectangle {
        anchors.centerIn: parent
        width: orbit.width * 0.27
        height: width
        radius: width * 0.18
        rotation: 45
        color: logo.inkColor
        opacity: logo.progress
        scale: 0.6 + 0.4 * logo.progress
    }

    Rectangle {
        anchors.centerIn: parent
        width: orbit.width * 0.11
        height: width
        radius: width / 2
        color: logo.primaryColor
        opacity: logo.progress
    }

    NumberAnimation {
        id: reveal
        target: logo
        property: "progress"
        from: 0
        to: 1
        duration: Theme.animations ? Math.max(240, Theme.animationDuration * 3) : 0
        easing.type: Easing.OutCubic
    }

    Component.onCompleted: restart()
    onAnimatedChanged: restart()
}
