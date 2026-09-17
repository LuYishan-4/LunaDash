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
    readonly property var panelModule: (((shell.state.shellModules || {}).modules || {}).panel || ({}))
    readonly property var panelConfig: panelModule.config || ({})
    readonly property int duration: panelConfig.transitionDuration ?? 420
    readonly property bool enabledTransition: (panelConfig.workspaceTransition ?? true) && Theme.animations
    property real phase: 0

    function replay() {
        if (!enabledTransition) return
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
        if (previousWorkspace === targetWorkspace) return
        previousWorkspace = targetWorkspace
        replay()
    }

    Rectangle {
        anchors.fill: parent
        color: Qt.rgba(Theme.background.r, Theme.background.g, Theme.background.b,
                       0.16 * Math.sin(Math.PI * overlay.phase))
    }

    Rectangle {
        id: beamLeft
        anchors.verticalCenter: parent.verticalCenter
        x: -width + (parent.width + width) * overlay.phase
        width: Math.max(120, parent.width * 0.16)
        height: parent.height * 1.35
        rotation: -12
        color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b,
                       0.16 * Math.sin(Math.PI * overlay.phase))
    }

    Rectangle {
        anchors.verticalCenter: parent.verticalCenter
        x: parent.width - ((parent.width + beamLeft.width) * overlay.phase)
        width: beamLeft.width
        height: beamLeft.height
        rotation: 12
        color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b,
                       0.10 * Math.sin(Math.PI * overlay.phase))
    }

    Item {
        anchors.centerIn: parent
        width: 190
        height: 190
        opacity: Math.sin(Math.PI * overlay.phase)
        scale: 0.48 + 0.90 * Math.sin(Math.PI * overlay.phase)
        rotation: -14 + 28 * overlay.phase
        LunaDashLogo {
            anchors.fill: parent
            animated: false
            primaryColor: Theme.accent
            secondaryColor: Qt.lighter(Theme.accent, 1.22)
            inkColor: Theme.text
        }
        Rectangle {
            anchors.centerIn: parent
            width: parent.width * (0.76 + 0.42 * overlay.phase)
            height: width
            radius: width / 2
            color: "transparent"
            border.width: 2
            border.color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b,
                                  0.38 * Math.sin(Math.PI * overlay.phase))
        }
    }

    SequentialAnimation {
        id: sequence
        NumberAnimation {
            target: overlay
            property: "phase"
            from: 0
            to: 1
            duration: Math.max(90, Math.round(overlay.duration * 0.52))
            easing.type: Easing.OutExpo
        }
        NumberAnimation {
            target: overlay
            property: "phase"
            from: 1
            to: 0
            duration: Math.max(90, Math.round(overlay.duration * 0.48))
            easing.type: Easing.InExpo
        }
        onFinished: overlay.running = false
    }
}
