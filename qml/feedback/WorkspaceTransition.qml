import QtQuick
import Quickshell
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
    readonly property int targetWorkspace: shell.state.workspace ?? 0
    property int pendingWorkspace: -1
    property int direction: 1
    property real phase: 0
    readonly property var panelModule: (((shell.state.shellModules || {}).modules || {}).panel || ({}))
    readonly property var panelConfig: panelModule.config || ({})
    readonly property int duration: Math.max(520, panelConfig.transitionDuration ?? 720)
    readonly property bool enabledTransition: (panelConfig.workspaceTransition ?? true) && Theme.animations
    readonly property real pulse: Math.sin(Math.PI * Math.min(1, phase))
    readonly property real transitionReveal: Math.sin(Math.PI * Math.min(1, phase * 0.92))

    function playTo(workspace) {
        if (previousWorkspace < 0) {
            previousWorkspace = workspace
            return
        }
        if (workspace === previousWorkspace)
            return

        if (running) {
            pendingWorkspace = workspace
            return
        }

        direction = workspace > previousWorkspace ? 1 : -1
        previousWorkspace = workspace
        phase = 0

        if (!enabledTransition) {
            running = false
            return
        }

        running = true
        sequence.restart()
    }

    onTargetWorkspaceChanged: playTo(targetWorkspace)
    Component.onCompleted: previousWorkspace = targetWorkspace

    Rectangle {
        anchors.fill: parent
        color: Qt.rgba(Theme.background.r, Theme.background.g, Theme.background.b,
                       0.26 * overlay.pulse)
    }

    Repeater {
        model: 4
        Rectangle {
            required property int index
            anchors.centerIn: parent
            width: Math.max(220, parent.width * (0.20 + index * 0.15))
            height: width
            radius: width / 2
            color: "transparent"
            border.width: Math.max(1, 4 - index)
            border.color: Qt.rgba(Theme.starlight.r, Theme.starlight.g, Theme.starlight.b,
                                  Math.max(0, 0.30 - index * 0.05) * overlay.pulse)
            scale: 0.44 + overlay.phase * (0.74 + index * 0.11)
            opacity: Math.max(0, overlay.pulse - index * 0.06)
        }
    }

    Rectangle {
        id: streak
        anchors.verticalCenter: parent.verticalCenter
        width: parent.width * 0.76
        height: 3
        radius: 2
        x: overlay.direction > 0
            ? -width + (parent.width + width) * overlay.phase
            : parent.width - (parent.width + width) * overlay.phase
        color: Qt.rgba(Theme.moon.r, Theme.moon.g, Theme.moon.b, 0.82 * overlay.pulse)
        opacity: overlay.pulse
    }

    Rectangle {
        anchors.verticalCenter: parent.verticalCenter
        width: streak.width * 0.52
        height: 1
        radius: 1
        x: overlay.direction > 0
            ? parent.width - (parent.width + width) * overlay.phase
            : -width + (parent.width + width) * overlay.phase
        color: Qt.rgba(Theme.starlight.r, Theme.starlight.g, Theme.starlight.b, 0.55 * overlay.pulse)
    }

    Item {
        id: logoStage
        anchors.centerIn: parent
        width: Math.min(230, Math.max(150, parent.width * 0.15))
        height: width
        opacity: overlay.transitionReveal
        scale: 0.72 + 0.30 * overlay.pulse
        rotation: overlay.direction * (-5 + 10 * overlay.phase)

        Rectangle {
            anchors.centerIn: parent
            width: parent.width * (1.18 + 0.20 * overlay.pulse)
            height: width
            radius: width / 2
            color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.05 * overlay.pulse)
            border.width: 2
            border.color: Qt.rgba(Theme.moon.r, Theme.moon.g, Theme.moon.b, 0.40 * overlay.pulse)
        }

        LunaDashLogo {
            anchors.fill: parent
            animated: false
            primaryColor: Theme.moon
            secondaryColor: Theme.starlight
            inkColor: Theme.text
        }
    }

    SequentialAnimation {
        id: sequence
        NumberAnimation {
            target: overlay
            property: "phase"
            from: 0
            to: 0.34
            duration: Math.round(overlay.duration * 0.34)
            easing.type: Easing.OutCubic
        }
        NumberAnimation {
            target: overlay
            property: "phase"
            from: 0.34
            to: 0.74
            duration: Math.round(overlay.duration * 0.32)
            easing.type: Easing.InOutCubic
        }
        NumberAnimation {
            target: overlay
            property: "phase"
            from: 0.74
            to: 1
            duration: Math.round(overlay.duration * 0.34)
            easing.type: Easing.InCubic
        }
        onFinished: {
            overlay.phase = 0
            overlay.running = false
            if (overlay.pendingWorkspace >= 0 && overlay.pendingWorkspace !== overlay.previousWorkspace) {
                const next = overlay.pendingWorkspace
                overlay.pendingWorkspace = -1
                Qt.callLater(function() { overlay.playTo(next) })
            } else {
                overlay.pendingWorkspace = -1
            }
        }
    }
}
