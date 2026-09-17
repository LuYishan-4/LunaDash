import QtQuick
import Quickshell.Wayland
import "../modules"
import "../components"
import "../style"

ModuleSurface {
    id: overlay
    moduleId: "feedback"
    anchors { top: true; right: true; bottom: true; left: true }
    exclusionMode: ExclusionMode.Ignore
    WlrLayershell.layer: WlrLayer.Overlay
    WlrLayershell.namespace: "lunadash-workspace-transition"
    implicitWidth: moduleWidth(screen ? screen.width : 1440)
    implicitHeight: moduleHeight(screen ? screen.height : 900)
    color: "transparent"
    visible: running

    property bool running: false
    property int previousWorkspace: -1
    property int targetWorkspace: shell.state.workspace ?? 0
    property int direction: 1
    property real phase: 0
    readonly property var panelModule: (((shell.state.shellModules || {}).modules || {}).panel || ({}))
    readonly property var panelConfig: panelModule.config || ({})
    readonly property int duration: Math.max(680, panelConfig.transitionDuration ?? 860)
    readonly property bool enabledTransition: (panelConfig.workspaceTransition ?? true) && Theme.animations
    readonly property real pulse: Math.sin(Math.PI * Math.min(1, phase))
    readonly property real reveal: Math.sin(Math.PI * Math.min(1, phase * 0.82))

    function replay() {
        if (!enabledTransition)
            return
        sequence.stop()
        phase = 0
        running = true
        sequence.start()
    }

    onTargetWorkspaceChanged: {
        if (previousWorkspace < 0) {
            previousWorkspace = targetWorkspace
            return
        }
        if (previousWorkspace === targetWorkspace)
            return
        direction = targetWorkspace > previousWorkspace ? 1 : -1
        previousWorkspace = targetWorkspace
        replay()
    }

    Rectangle {
        anchors.fill: parent
        color: Qt.rgba(Theme.background.r, Theme.background.g, Theme.background.b,
                       0.30 * overlay.pulse)
    }

    // A soft moonlit tunnel replaces the old two fast rectangular beams.
    Repeater {
        model: 5
        Rectangle {
            required property int index
            anchors.centerIn: parent
            width: Math.max(220, parent.width * (0.18 + index * 0.13))
            height: width
            radius: width / 2
            color: "transparent"
            border.width: Math.max(1, 5 - index)
            border.color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b,
                                  Math.max(0, 0.34 - index * 0.045) * overlay.pulse)
            scale: 0.38 + overlay.phase * (0.82 + index * 0.12)
            opacity: Math.max(0, overlay.pulse - index * 0.045)
        }
    }

    Rectangle {
        id: streak
        anchors.verticalCenter: parent.verticalCenter
        width: parent.width * 0.72
        height: 3
        radius: 2
        x: overlay.direction > 0
            ? -width + (parent.width + width) * overlay.phase
            : parent.width - (parent.width + width) * overlay.phase
        color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.74 * overlay.pulse)
        opacity: overlay.pulse
    }

    Rectangle {
        anchors.verticalCenter: parent.verticalCenter
        width: streak.width * 0.55
        height: 1
        radius: 1
        x: overlay.direction > 0
            ? parent.width - (parent.width + width) * overlay.phase
            : -width + (parent.width + width) * overlay.phase
        color: Qt.rgba(Theme.text.r, Theme.text.g, Theme.text.b, 0.42 * overlay.pulse)
    }

    Item {
        id: logoStage
        anchors.centerIn: parent
        width: Math.min(250, Math.max(160, parent.width * 0.16))
        height: width
        opacity: overlay.reveal
        scale: 0.68 + 0.38 * overlay.pulse
        rotation: overlay.direction * (-7 + 14 * overlay.phase)

        Rectangle {
            anchors.centerIn: parent
            width: parent.width * (1.16 + 0.28 * overlay.pulse)
            height: width
            radius: width / 2
            color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.055 * overlay.pulse)
            border.width: 2
            border.color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.42 * overlay.pulse)
        }

        LunaDashLogo {
            anchors.fill: parent
            animated: false
            primaryColor: Theme.accent
            secondaryColor: Qt.lighter(Theme.accent, 1.26)
            inkColor: Theme.text
        }
    }

    SequentialAnimation {
        id: sequence
        NumberAnimation {
            target: overlay
            property: "phase"
            from: 0
            to: 0.28
            duration: Math.round(overlay.duration * 0.28)
            easing.type: Easing.OutCubic
        }
        PauseAnimation { duration: Math.round(overlay.duration * 0.14) }
        NumberAnimation {
            target: overlay
            property: "phase"
            from: 0.28
            to: 0.72
            duration: Math.round(overlay.duration * 0.30)
            easing.type: Easing.InOutCubic
        }
        NumberAnimation {
            target: overlay
            property: "phase"
            from: 0.72
            to: 1
            duration: Math.round(overlay.duration * 0.28)
            easing.type: Easing.InCubic
        }
        onFinished: {
            overlay.phase = 0
            overlay.running = false
        }
    }
}
