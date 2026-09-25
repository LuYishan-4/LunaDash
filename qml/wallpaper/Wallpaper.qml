import "../modules"
import "../plugins"
import QtQuick
import Quickshell
import Quickshell.Wayland
import "../style"

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

    readonly property bool ready: replacementReady || background.ready
    readonly property string stateSource: String(wallpaper.shell.state.wallpaperImage || "")
    readonly property var media: ((shell.state.wallpapers || {}).current || {})
    readonly property bool live: media.type === "video" && !shell.wallpaperOverride.length
    property bool liveLayerActive: live
    readonly property string desiredSource: wallpaper.shell.wallpaperOverride.length
        ? wallpaper.shell.wallpaperOverride : stateSource

    Rectangle {
        anchors.fill: parent
        color: moduleStyle.background === "inherit" ? "transparent" : moduleBackground
        radius: moduleRadius
    }

    WallpaperTransition {
        id: background
        shell: wallpaper.shell
        anchors.fill: parent
        source: wallpaper.live ? (wallpaper.media.preview || "") : wallpaper.desiredSource
        pixelRatio: wallpaper.screen ? wallpaper.screen.devicePixelRatio : 1
    }
    onLiveChanged: {
        if (live) {
            liveUnload.stop()
            liveLayerActive = true
        } else if (liveLayerActive) {
            if (Theme.animations && Theme.animationDuration > 0)
                liveUnload.restart()
            else
                liveLayerActive = false
        }
    }

    Timer {
        id: liveUnload
        interval: Math.max(180, Theme.animationDuration)
        repeat: false
        onTriggered: if (!wallpaper.live) wallpaper.liveLayerActive = false
    }

    Loader {
        id: liveLoader
        anchors.fill: parent
        active: wallpaper.liveLayerActive
        source: active ? "LiveWallpaper.qml" : ""
        opacity: wallpaper.live && status === Loader.Ready && !background.transitioning ? 1 : 0
        scale: opacity > 0.5 ? 1 : 1.015
        Behavior on opacity {
            enabled: Theme.animations && Theme.animationDuration > 0
            NumberAnimation { duration: Math.max(180, Theme.animationDuration); easing.type: Easing.InOutCubic }
        }
        Behavior on scale {
            enabled: Theme.animations && Theme.animationDuration > 0
            NumberAnimation { duration: Math.max(180, Theme.animationDuration); easing.type: Easing.OutCubic }
        }
        onLoaded: {
            item.source = Qt.binding(() => wallpaper.live ? (wallpaper.media.url || "") : "")
            item.playing = Qt.binding(() => wallpaper.live && !wallpaper.shell.stopping)
            item.animationsEnabled = Qt.binding(() => Theme.animations)
            item.transitionDuration = Qt.binding(() => Math.max(180, Theme.animationDuration))
        }
        onStatusChanged: if (status === Loader.Error)
            wallpaper.shell.notify(wallpaper.shell.tr("Live wallpaper"), wallpaper.shell.tr("Install Qt Multimedia to play video wallpapers."), "error", "")
        Connections {
            target: liveLoader.item
            function onFailed(message) { wallpaper.shell.notify(wallpaper.shell.tr("Live wallpaper"), message, "error", "") }
        }
    }
    Rectangle {
        anchors.fill: parent
        color: "#0b1720"
        opacity: 0.12
        visible: background.displayedSource.toString().length > 0
    }

    // Desktop widgets render inside the wallpaper's Background layer. Plugins
    // can no longer accidentally create an always-on-top desktop widget that
    // remains visible over application windows.
    ExtensionSlot {
        id: desktopWidgets
        anchors.fill: parent
        shell: wallpaper.shell
        target: "desktop-widgets"
        context: ({
            wallpaper: wallpaper,
            screen: wallpaper.screen,
            layer: "background"
        })
        z: 2
        DesktopWidgets {
            anchors.fill: parent
            shell: wallpaper.shell
            settings: desktopWidgets.targetSpec.builtinSettings || ({})
        }
    }

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.RightButton
        onClicked: mouse => wallpaper.shell.openMenu(mouse.x, mouse.y)
    }
}
