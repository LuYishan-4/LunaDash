import QtQuick
import "../plugins"
import QtQuick.Effects
import Quickshell
import Quickshell.Wayland
import "../style"

PluginPanel {
    id: panel
    extensionTarget: interaction.scope === "windows" ? "window-switcher" : "workspace-switcher"
    extensionContext: ({interaction: interaction, colors: Theme.palette,
        accent: Theme.accent, background: Theme.surfaceStrong, foreground: Theme.text,
        muted: Theme.muted, fontFamily: Theme.font})
    required property var interaction
    readonly property bool opened: Boolean(interaction.active && interaction.ready) && !shell.stopping
    property real reveal: opened ? 1 : 0
    visible: opened || reveal > 0
    anchors { top: true; bottom: true; left: true; right: true }
    exclusionMode: ExclusionMode.Ignore
    WlrLayershell.layer: WlrLayer.Overlay
    WlrLayershell.keyboardFocus: WlrKeyboardFocus.None
    WlrLayershell.namespace: "lunadash-window-switcher"
    color: "transparent"
    Behavior on reveal { NumberAnimation { duration: Theme.motion; easing.type: Easing.OutCubic } }
    Item {
        anchors.fill: parent
        opacity: panel.reveal
        Image {
            id: background
            anchors.fill: parent
            source: panel.interaction.background || panel.shell.state.wallpaperMedia?.poster || (panel.shell.state.wallpaperMedia?.type === "video" ? "" : panel.shell.state.wallpaperImage) || ""
            fillMode: Image.PreserveAspectCrop
            visible: false
            cache: false
        }
        ExtensionSlot {
            anchors.fill: parent
            shell: panel.shell
            target: "blur"
            context: ({image: background, reveal: panel.reveal})
        MultiEffect {
            anchors.fill: parent
            source: background
            blurEnabled: true
            blurMax: 24
            blur: panel.reveal
            saturation: -0.25
        }
        }
        Rectangle { anchors.fill: parent; color: Theme.background; opacity: 0.18 }
        MouseArea { anchors.fill: parent; onClicked: panel.shell.command("switch-cancel", "") }
        Loader {
            sourceComponent: panel.interaction.scope === "windows" ? windowTemplate : workspaceTemplate
            anchors.horizontalCenter: parent.horizontalCenter
            y: Math.min(Math.max(64, parent.height * 0.12), parent.height - height - 20)
            width: Math.max(240, parent.width - Math.min(96, parent.width * 0.08))
            height: panel.interaction.scope === "windows"
                ? Math.min(parent.height - 80, 380)
                : Math.min(parent.height - 80, (width - 10) * 0.225 + 10)
            scale: 0.98 + 0.02 * panel.reveal
            transform: Translate { y: -10 * (1 - panel.reveal) }
        }
    }
    Component {
        id: workspaceTemplate
        SwitcherContent { shell: panel.shell; selection: panel.interaction }
    }
    Component {
        id: windowTemplate
        WindowPreviewContent { shell: panel.shell; selection: panel.interaction }
    }
}
