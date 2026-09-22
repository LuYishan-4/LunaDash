import "../modules"
import "../plugins"
import QtQuick
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

    readonly property bool ready: replacementReady || background.ready
    readonly property string stateSource: String(wallpaper.shell.state.wallpaperImage || "")
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
        source: wallpaper.desiredSource
        pixelRatio: wallpaper.screen ? wallpaper.screen.devicePixelRatio : 1
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
    }

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.RightButton
        onClicked: mouse => wallpaper.shell.openMenu(mouse.x, mouse.y)
    }
}
