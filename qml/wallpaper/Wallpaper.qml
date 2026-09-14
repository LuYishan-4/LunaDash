import "../modules"
import QtQuick
import Quickshell
import Quickshell.Wayland
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
    Rectangle { anchors.fill: parent; color: moduleStyle.background === "inherit" ? "transparent" : moduleBackground; radius: moduleRadius }
    Image {
        anchors.fill: parent
        source: wallpaper.shell.state.wallpaperImage || ""
        fillMode: Image.PreserveAspectCrop
        asynchronous: true
        sourceSize.width: 2560
        sourceSize.height: 1600
        cache: false
    }
    Rectangle { anchors.fill: parent; color: "#0b1720"; opacity: 0.12; visible: Boolean(wallpaper.shell.state.wallpaperImage) }
    MouseArea { anchors.fill: parent; acceptedButtons: Qt.RightButton; onClicked: mouse => wallpaper.shell.openMenu(mouse.x, mouse.y) }
}
