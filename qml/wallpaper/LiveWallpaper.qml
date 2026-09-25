import QtQuick
import QtMultimedia

Item {
    id: root
    property url source
    property bool playing: true
    property bool animationsEnabled: true
    property int transitionDuration: 220
    signal failed(string message)

    function revealVideo() {
        entrance.stop()
        if (!source.toString().length) {
            video.opacity = 0
            return
        }
        if (!animationsEnabled || transitionDuration <= 0) {
            video.opacity = 1
            video.scale = 1
            return
        }
        entrance.restart()
    }

    MediaPlayer {
        id: player
        source: root.source
        loops: MediaPlayer.Infinite
        videoOutput: video
        audioOutput: AudioOutput {
            muted: true
        }
        onSourceChanged: {
            entrance.stop()
            video.opacity = root.animationsEnabled ? 0 : 1
            video.scale = root.animationsEnabled ? 1.02 : 1
            if (root.playing && source.toString().length)
                play()
            else if (!source.toString().length)
                stop()
        }
        onMediaStatusChanged: {
            if (mediaStatus === MediaPlayer.LoadedMedia ||
                mediaStatus === MediaPlayer.BufferedMedia)
                root.revealVideo()
        }
        onErrorOccurred: (error, errorString) => root.failed(errorString)
    }
    VideoOutput {
        id: video
        anchors.fill: parent
        fillMode: VideoOutput.PreserveAspectCrop
        opacity: root.animationsEnabled ? 0 : 1
        scale: root.animationsEnabled ? 1.02 : 1
    }
    ParallelAnimation {
        id: entrance
        NumberAnimation {
            target: video
            property: "opacity"
            from: 0
            to: 1
            duration: root.transitionDuration
            easing.type: Easing.InOutCubic
        }
        NumberAnimation {
            target: video
            property: "scale"
            from: 1.02
            to: 1
            duration: root.transitionDuration
            easing.type: Easing.OutCubic
        }
    }
    onPlayingChanged: {
        if (!source.toString().length)
            return
        playing ? player.play() : player.pause()
    }
    onSourceChanged: {
        entrance.stop()
        video.opacity = animationsEnabled ? 0 : 1
        video.scale = animationsEnabled ? 1.02 : 1
    }
    Component.onCompleted: if (playing && source.toString().length)
        player.play()
    Component.onDestruction: player.stop()
}
