import QtQuick
import "../components"
import "../style"

Item {
    id: disc
    property url artwork: ""
    property bool playing: false
    property real progress: 0
    property real phase: 0
    implicitWidth: 240
    implicitHeight: 240

    Canvas {
        id: ring
        anchors.fill: parent
        onPaint: {
            const ctx = getContext("2d")
            ctx.reset()
            const cx = width / 2, cy = height / 2, radius = Math.min(width, height) * 0.35
            ctx.lineCap = "round"
            ctx.lineWidth = Math.max(2, width / 62)
            for (let i = 0; i < 56; ++i) {
                const angle = i / 56 * Math.PI * 2 - Math.PI / 2
                const wave = disc.playing ? (Math.sin(i * 0.47 + disc.phase * Math.PI * 2) + 1) * 0.5 : 0.15
                const length = width * (0.025 + 0.075 * wave * wave)
                ctx.strokeStyle = i / 56 <= disc.progress ? Theme.accent : Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.4)
                ctx.beginPath()
                ctx.moveTo(cx + Math.cos(angle) * radius, cy + Math.sin(angle) * radius)
                ctx.lineTo(cx + Math.cos(angle) * (radius + length), cy + Math.sin(angle) * (radius + length))
                ctx.stroke()
            }
        }
    }
    Rectangle {
        anchors.centerIn: parent
        width: parent.width * 0.62
        height: width
        radius: width / 2
        color: Qt.rgba(Theme.secondaryAccent.r, Theme.secondaryAccent.g, Theme.secondaryAccent.b, 0.3)
        LineIcon { anchors.centerIn: parent; width: parent.width * 0.4; height: width; name: "music"; ink: Theme.starlight }
    }
    Image { id: art; source: disc.artwork; visible: false; asynchronous: true; sourceSize: Qt.size(480, 480) }
    Canvas {
        id: cover
        anchors.fill: parent
        property url loadedSource: ""
        onImageLoaded: requestPaint()
        onPaint: {
            const ctx = getContext("2d")
            ctx.reset()
            if (!isImageLoaded(disc.artwork) || art.status !== Image.Ready)
                return
            const diameter = width * 0.62, scale = Math.max(diameter / art.implicitWidth, diameter / art.implicitHeight)
            ctx.save()
            ctx.beginPath()
            ctx.arc(width / 2, height / 2, diameter / 2, 0, Math.PI * 2)
            ctx.clip()
            ctx.drawImage(disc.artwork, (width - art.implicitWidth * scale) / 2, (height - art.implicitHeight * scale) / 2,
                          art.implicitWidth * scale, art.implicitHeight * scale)
            ctx.restore()
        }
    }
    onArtworkChanged: {
        if (cover.loadedSource.toString().length)
            cover.unloadImage(cover.loadedSource)
        cover.loadedSource = artwork
        if (artwork.toString().length)
            cover.loadImage(artwork)
        cover.requestPaint()
    }
    Connections { target: art; function onStatusChanged() { cover.requestPaint() } }
    onWidthChanged: { ring.requestPaint(); cover.requestPaint() }
    onHeightChanged: { ring.requestPaint(); cover.requestPaint() }
    onPhaseChanged: ring.requestPaint()
    onProgressChanged: ring.requestPaint()
    onPlayingChanged: ring.requestPaint()
    Connections { target: Theme; function onAccentChanged() { ring.requestPaint() } }
    NumberAnimation on phase {
        from: 0; to: 1; duration: 2400; loops: Animation.Infinite
        running: disc.visible && disc.playing && Theme.animations
    }
}
