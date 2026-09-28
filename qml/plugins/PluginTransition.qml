import QtQuick
import Quickshell
import Quickshell.Wayland
import "ActivationChanges.js" as ActivationChanges
import "../style"

// A single clock drives all outputs. Plugin contents never own timing or input.
Scope {
    id: transition
    required property var shell
    property var queue: []
    property var change: ({})
    property real progress: 0
    property bool running: false
    readonly property bool motionAllowed: !shell.stopping && Theme.animations && Theme.animationDuration > 0
    readonly property int duration: Math.max(100, Math.min(1600,
        ((shell.state.extensions || {}).targets || []).find(entry => entry.id === "plugin-transition")?.builtinSettings?.duration ?? 420))
    function observe(previous, next) {
        if (!motionAllowed) return
        const changed = ActivationChanges.changes(previous, next)
        if (!changed.length) return
        queue = queue.concat(changed)
        if (!running) playNext()
    }
    function playNext() {
        if (!motionAllowed || !queue.length) { running = false; return }
        change = queue[0]
        queue = queue.slice(1)
        progress = 0
        running = true
        animation.restart()
    }
    onMotionAllowedChanged: if (!motionAllowed) {
        animation.stop()
        queue = []
        running = false
        progress = 0
    }
    NumberAnimation {
        id: animation
        target: transition
        property: "progress"
        from: 0; to: 1
        duration: transition.duration
        easing.type: Easing.InOutCubic
        onFinished: {
            transition.running = false
            Qt.callLater(transition.playNext)
        }
    }
    Variants {
        model: Quickshell.screens
        delegate: PanelWindow {
            id: overlay
            required property var modelData
            screen: modelData
            anchors { top: true; bottom: true; left: true; right: true }
            exclusionMode: ExclusionMode.Ignore
            WlrLayershell.layer: WlrLayer.Overlay
            WlrLayershell.keyboardFocus: WlrKeyboardFocus.None
            WlrLayershell.namespace: "lunadash-plugin-transition"
            color: "transparent"
            visible: transition.running
            mask: Region {}
            ExtensionSlot {
                anchors.fill: parent
                shell: transition.shell
                target: "plugin-transition"
                context: ({progress: transition.progress, change: transition.change,
                    duration: transition.duration, background: Theme.background,
                    accent: Theme.accent, foreground: Theme.text, fontFamily: Theme.font})
                PluginTransitionContent {
                    anchors.fill: parent
                    progress: transition.progress
                    label: transition.change.name || ""
                }
            }
        }
    }
}
