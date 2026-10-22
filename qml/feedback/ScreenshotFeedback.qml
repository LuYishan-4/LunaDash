import QtQuick
import Quickshell
import Quickshell.Wayland
import "../modules"
import "../style"

ModuleSurface {
    id: overlay
    moduleId: "feedback"
    extensionTarget: "screenshot"
    anchors { top: true; right: true; bottom: true; left: true }
    exclusionMode: ExclusionMode.Ignore
    WlrLayershell.layer: WlrLayer.Overlay
    WlrLayershell.namespace: "lunadash-screenshot-feedback"
    implicitWidth: moduleWidth(screen ? screen.width : 1440)
    implicitHeight: moduleHeight(screen ? screen.height : 900)
    color: "transparent"
    visible: running

    property bool running: false
    property real phase: 0
    property int serial: shell.screenshotSerial

    onSerialChanged: {
        if (serial <= 0 || !Theme.animations)
            return
        sequence.stop()
        phase = 0
        running = true
        sequence.start()
    }

    Rectangle {
        anchors.fill: parent
        color: Qt.rgba(Theme.background.r, Theme.background.g, Theme.background.b,
                       0.16 * Math.sin(Math.PI * overlay.phase))
    }

    Rectangle {
        anchors.centerIn: parent
        width: parent.width * (0.72 + 0.24 * overlay.phase)
        height: parent.height * (0.68 + 0.26 * overlay.phase)
        radius: 24
        color: "transparent"
        border.width: 2
        border.color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b,
                              0.88 * Math.sin(Math.PI * overlay.phase))
        opacity: Math.min(1, overlay.phase * 3) * (1 - Math.max(0, overlay.phase - 0.76) / 0.24)
        Behavior on width { NumberAnimation { duration: Theme.motion; easing.type: Easing.OutCubic } }
    }

    Rectangle {
        width: parent.width * 0.76
        height: 3
        radius: 2
        x: (parent.width - width) / 2
        y: parent.height * (0.16 + 0.68 * overlay.phase)
        color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b,
                       0.72 * Math.sin(Math.PI * overlay.phase))
        visible: overlay.phase > 0.08 && overlay.phase < 0.92
    }

    SequentialAnimation {
        id: sequence
        NumberAnimation { target: overlay; property: "phase"; from: 0; to: 0.62; duration: 260; easing.type: Easing.OutCubic }
        PauseAnimation { duration: 120 }
        NumberAnimation { target: overlay; property: "phase"; from: 0.62; to: 1; duration: 260; easing.type: Easing.InCubic }
        onFinished: {
            overlay.running = false
            overlay.phase = 0
        }
    }
}
