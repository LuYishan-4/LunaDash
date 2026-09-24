import QtQuick
import QtMultimedia

Item {
    id: root
    property url source
    property bool playing: true
    signal failed(string message)
    MediaPlayer {
        id: player
        source: root.source
        loops: MediaPlayer.Infinite
        videoOutput: video
        audioOutput: AudioOutput {
            muted: true
        }
        onSourceChanged: if (root.playing && source.toString().length)
            play()
        onErrorOccurred: (error, errorString) => root.failed(errorString)
    }
    VideoOutput {
        id: video
        anchors.fill: parent
        fillMode: VideoOutput.PreserveAspectCrop
    }
    onPlayingChanged: playing ? player.play() : player.pause()
    Component.onCompleted: if (playing)
        player.play()
    Component.onDestruction: player.stop()
}
