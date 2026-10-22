import QtQuick
import QtQuick.Window
import "../style"
import "../plugins"

ExtensionSlot {
    id: transition
    target: "wallpaper-transition"
    context: ({imageSource: source, previous: currentImage, incoming: incomingImage, progress: progress, transitioning: transitioning})
    property url source: ""
    property real pixelRatio: 1
    property bool animationsEnabled: Theme.animations && Theme.animationDuration > 0
    property int duration: Math.max(620, Theme.animationDuration * 2.7)
    property real progress: 1
    property int currentIndex: 0
    property bool transitioning: false
    property bool initialized: false
    property bool failed: false
    readonly property var currentImage: currentIndex === 0 ? first : second
    readonly property var incomingImage: currentIndex === 0 ? second : first
    readonly property url displayedSource: currentImage.source
    readonly property bool ready: currentImage.status === Image.Ready || failed || !source.toString().length
    readonly property bool software: GraphicsInfo.api === GraphicsInfo.Software || GraphicsInfo.api === GraphicsInfo.Unknown
    signal sourceFailed(url failedSource)
    clip: true

    function requestSource() {
        if (!initialized || transitioning)
            return
        failed = false
        if (source.toString() === currentImage.source.toString()) {
            incomingImage.source = ""
            return
        }
        if (!source.toString().length) {
            currentImage.source = ""
            incomingImage.source = ""
            return
        }
        incomingImage.source = source
        beginIfReady()
    }

    function beginIfReady() {
        if (!initialized || transitioning || incomingImage.source.toString() !== source.toString())
            return
        if (incomingImage.status === Image.Error) {
            failed = true
            sourceFailed(source)
            return
        }
        if (incomingImage.status !== Image.Ready)
            return
        if (currentImage.status !== Image.Ready || !animationsEnabled) {
            finishTransition()
            return
        }
        if (reveal.status !== Loader.Ready || !reveal.item.prepared)
            return
        progress = 0
        transitioning = true
        expansion.restart()
    }

    function finishTransition() {
        expansion.stop()
        currentIndex = 1 - currentIndex
        transitioning = false
        progress = 1
        // Swap loaded image buffers instead of reloading the new background.
        Qt.callLater(function() {
            if (!transition.transitioning) {
                transition.incomingImage.source = ""
                transition.requestSource()
            }
        })
    }

    onSourceChanged: requestSource()
    onAnimationsEnabledChanged: {
        if (!animationsEnabled && transitioning)
            finishTransition()
    }
    Component.onCompleted: { initialized = true; requestSource() }

    Image {
        id: first
        anchors.fill: parent
        asynchronous: true
        cache: true
        fillMode: Image.PreserveAspectCrop
        sourceSize: Qt.size(Math.ceil(transition.width * transition.pixelRatio), Math.ceil(transition.height * transition.pixelRatio))
        visible: transition.currentIndex === 0
        onStatusChanged: transition.beginIfReady()
    }
    Image {
        id: second
        anchors.fill: parent
        asynchronous: true
        cache: true
        fillMode: Image.PreserveAspectCrop
        sourceSize: first.sourceSize
        visible: transition.currentIndex === 1
        onStatusChanged: transition.beginIfReady()
    }
    Loader {
        id: reveal
        anchors.fill: parent
        active: transition.incomingImage.status === Image.Ready && transition.currentImage.status === Image.Ready
        visible: transition.transitioning
        source: transition.software ? "CircularRevealSoftware.qml" : "CircularRevealGpu.qml"
        onLoaded: transition.beginIfReady()
    }
    Binding { target: reveal.item; property: "sourceItem"; value: transition.incomingImage; when: reveal.status === Loader.Ready }
    Binding { target: reveal.item; property: "progress"; value: transition.progress; when: reveal.status === Loader.Ready }
    Connections {
        target: reveal.item
        function onPreparedChanged() { transition.beginIfReady() }
    }
    NumberAnimation {
        id: expansion
        target: transition
        property: "progress"
        from: 0
        to: 1
        duration: transition.duration
        easing.type: Easing.InOutCubic
        onFinished: transition.finishTransition()
    }
}
