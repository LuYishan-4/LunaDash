import QtQuick
import Quickshell
import Quickshell.Wayland
import "../modules"
import "../components"
import "../style"

ModuleSurface {
    id: overlay
    moduleId: "startup"
    extensionTarget: "startup"
    signal finished()
    anchors { top: true; right: true; bottom: true; left: true }
    exclusionMode: ExclusionMode.Ignore
    WlrLayershell.layer: WlrLayer.Overlay
    WlrLayershell.namespace: "lunadash-startup"
    implicitWidth: moduleWidth(screen ? screen.width : 1440)
    implicitHeight: moduleHeight(screen ? screen.height : 900)
    color: Qt.rgba(Theme.background.r, Theme.background.g, Theme.background.b, 0.96)
    property bool motionEnabled: Theme.animations && ((shell.state.appearance || {}).animations ?? true)
    property bool finishing: false
    property real fadeOpacity: 1
    contentItem.opacity: fadeOpacity

    function finish() {
        if (finishing) return
        finishing = true
        if (motionEnabled) fadeOut.start()
        else finished()
    }

    Item {
        id: lockup
        anchors.centerIn: parent
        width: 260; height: 210
        opacity: 0; scale: 0.92
        LunaDashLogo { id: mark; anchors.horizontalCenter: parent.horizontalCenter; width: 132; height: 132; animated: overlay.motionEnabled }
        Text {
            anchors.top: mark.bottom; anchors.topMargin: 16; anchors.horizontalCenter: parent.horizontalCenter
            text: "LunaDash"; color: Theme.text; font.family: Theme.font; font.pixelSize: 25; font.letterSpacing: 3
        }
    }
    ParallelAnimation {
        id: reveal
        NumberAnimation { target: lockup; property: "opacity"; from: 0; to: 1; duration: overlay.motionEnabled ? Math.max(180, Theme.animationDuration * 2) : 0; easing.type: Easing.OutCubic }
        NumberAnimation { target: lockup; property: "scale"; from: 0.92; to: 1; duration: overlay.motionEnabled ? Math.max(180, Theme.animationDuration * 2) : 0; easing.type: Easing.OutBack }
        onFinished: hold.start()
    }
    PauseAnimation { id: hold; duration: overlay.motionEnabled ? Math.max(260, Theme.animationDuration) : 0; onFinished: overlay.finish() }
    NumberAnimation {
        id: fadeOut; target: overlay; property: "fadeOpacity"; from: 1; to: 0
        duration: overlay.motionEnabled ? Math.max(120, Theme.animationDuration) : 0; easing.type: Easing.InCubic
        onFinished: overlay.finished()
    }
    Timer { interval: 3000; running: true; repeat: false; onTriggered: overlay.finished() }
    Component.onCompleted: {
        if (motionEnabled) reveal.start()
        else Qt.callLater(finished)
    }
}
