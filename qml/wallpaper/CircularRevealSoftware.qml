import QtQuick

Canvas {
    id: reveal
    property var sourceItem: null
    property real progress: 0
    readonly property url imageSource: sourceItem ? sourceItem.source : ""
    property bool prepared: false
    property url previousSource: ""
    renderTarget: Canvas.Image
    onImageSourceChanged: {
        prepared = false
        if (previousSource.toString().length)
            unloadImage(previousSource)
        previousSource = imageSource
        if (imageSource.toString().length) {
            loadImage(imageSource)
            prepared = isImageLoaded(imageSource)
        }
        requestPaint()
    }
    onImageLoaded: { prepared = isImageLoaded(imageSource); requestPaint() }
    onProgressChanged: requestPaint()
    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()
    onPaint: {
        const ctx = getContext("2d")
        ctx.reset()
        if (!prepared || !sourceItem || width <= 0 || height <= 0)
            return
        const iw = Math.max(1, sourceItem.implicitWidth)
        const ih = Math.max(1, sourceItem.implicitHeight)
        const scale = Math.max(width / iw, height / ih)
        ctx.save()
        ctx.beginPath()
        ctx.arc(width, height, Math.hypot(width, height) * progress, 0, Math.PI * 2)
        ctx.clip()
        ctx.drawImage(imageSource, (width - iw * scale) / 2, (height - ih * scale) / 2, iw * scale, ih * scale)
        ctx.restore()
    }
}
