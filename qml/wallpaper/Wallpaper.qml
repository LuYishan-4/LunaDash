import QtQuick
import Quickshell
import Quickshell.Wayland
PanelWindow {
    id: wallpaper
    required property var shell
    anchors { top: true; bottom: true; left: true; right: true }
    WlrLayershell.layer: WlrLayer.Background
    WlrLayershell.namespace: "ludash-wallpaper"
    exclusionMode: ExclusionMode.Ignore
    color: "transparent"
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
    MouseArea { anchors.fill: parent; acceptedButtons: Qt.RightButton; onClicked: wallpaper.shell.settingsOpen = !wallpaper.shell.settingsOpen }
}
