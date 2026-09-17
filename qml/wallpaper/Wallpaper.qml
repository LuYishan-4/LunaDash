import "../modules"
import "../plugins"
import QtQuick
import QtQuick.Effects
import Quickshell
import Quickshell.Wayland
import "../style" as Style

ModuleSurface {
    id: wallpaper
    moduleId: "wallpaper"
    anchors { top: !moduleStyle.height; bottom: !moduleStyle.height; left: !moduleStyle.width; right: !moduleStyle.width }
    implicitWidth: moduleWidth(screen ? screen.width : 1440)
    implicitHeight: moduleHeight(screen ? screen.height : 900)
    margins { top: moduleMargin; bottom: moduleMargin; left: moduleMargin; right: moduleMargin }
    WlrLayershell.layer: WlrLayer.Background
    WlrLayershell.namespace: "lunadash-wallpaper"
    exclusionMode: ExclusionMode.Ignore
    color: "transparent"

    property string displayedSource: ""
    property string incomingSource: ""
    property string queuedSource: ""
    property real revealProgress: 1
    property bool transitioning: false
    readonly property string stateSource: String(wallpaper.shell.state.wallpaperImage || "")
    readonly property string desiredSource: wallpaper.shell.wallpaperOverride.length
        ? wallpaper.shell.wallpaperOverride
        : stateSource

    function commitImmediately(source) {
        revealAnimation.stop()
        displayedSource = source
        incomingSource = ""
        queuedSource = ""
        transitioning = false
        revealProgress = 1
    }

    function requestSource(source) {
        source = String(source || "")

        if (!source.length) {
            commitImmediately("")
            return
        }

        if (!displayedSource.length) {
            commitImmediately(source)
            return
        }

        if (source === displayedSource || source === incomingSource) {
            queuedSource = ""
            return
        }

        if (transitioning) {
            queuedSource = source
            return
        }

        incomingSource = source
        queuedSource = ""
        revealProgress = 0

        if (!Style.Theme.animations) {
            commitImmediately(source)
            return
        }

        transitioning = true
        revealAnimation.restart()
    }

    function finishTransition() {
        const completed = incomingSource
        displayedSource = completed
        incomingSource = ""
        transitioning = false
        revealProgress = 1

        const next = queuedSource
        queuedSource = ""
        if (next.length && next !== completed)
            Qt.callLater(function() { wallpaper.requestSource(next) })
    }

    onDesiredSourceChanged: requestSource(desiredSource)
    Component.onCompleted: commitImmediately(desiredSource)

    Rectangle {
        anchors.fill: parent
        color: moduleStyle.background === "inherit" ? "transparent" : moduleBackground
        radius: moduleRadius
    }

    Image {
        id: baseImage
        anchors.fill: parent
        source: wallpaper.displayedSource
        fillMode: Image.PreserveAspectCrop
        asynchronous: true
        sourceSize.width: 2560
        sourceSize.height: 1600
        cache: true
    }

    Image {
        id: nextImage
        anchors.fill: parent
        source: wallpaper.incomingSource
        fillMode: Image.PreserveAspectCrop
        asynchronous: true
        sourceSize.width: 2560
        sourceSize.height: 1600
        cache: true
        visible: wallpaper.transitioning && wallpaper.incomingSource.length > 0
    }

    Item {
        id: revealMask
        anchors.fill: parent
        layer.enabled: true
        visible: false
        Rectangle {
            anchors.centerIn: parent
            width: Math.max(1, Math.hypot(revealMask.width, revealMask.height) * 2.08 * wallpaper.revealProgress)
            height: width
            radius: width / 2
            color: "white"
        }
    }

    MultiEffect {
        anchors.fill: parent
        source: nextImage
        maskEnabled: true
        maskSource: revealMask
        maskThresholdMin: 0.45
        maskSpreadAtMin: 0.02
        visible: wallpaper.transitioning && wallpaper.incomingSource.length > 0
    }

    Rectangle {
        anchors.fill: parent
        color: "#0b1720"
        opacity: 0.12
        visible: Boolean(wallpaper.displayedSource || wallpaper.incomingSource)
    }

    NumberAnimation {
        id: revealAnimation
        target: wallpaper
        property: "revealProgress"
        from: 0
        to: 1
        duration: Math.max(620, Style.Theme.animationDuration * 2.7)
        easing.type: Easing.OutCubic
        onFinished: wallpaper.finishTransition()
    }

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.RightButton
        onClicked: mouse => wallpaper.shell.openMenu(mouse.x, mouse.y)
    }
    PluginHost { shell: wallpaper.shell }
}
