import QtQuick
import "../style"

Item {
    id: mascot
    property bool playing: false
    implicitWidth: 190
    implicitHeight: 190
    AnimatedImage {
        anchors.centerIn: parent
        width: parent.width
        height: parent.height
        fillMode: Image.PreserveAspectFit
        source: "assets/togawa-sakiko-ave-mujica.gif"
        cache: false
        playing: true
        paused: !mascot.visible || !mascot.playing || !Theme.animations
    }
}
