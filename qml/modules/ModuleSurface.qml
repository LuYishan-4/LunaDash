import QtQuick
import "../components"
import "../style"
import "../plugins"

AnimatedPanel {
    id: host
    required property string moduleId
    required property var shell
    property string extensionTarget: moduleId
    property var extensionContext: ({
            module: host,
            moduleId: moduleId,
            style: resolvedStyle,
            config: specification.config || {},
            opened: opened
        })
    readonly property alias replacementReady: extension.replacementReady
    default property alias moduleData: extension.builtinData
    readonly property var specification: ((shell.state.shellModules || {}).modules || {})[moduleId] || ({})
    readonly property var moduleStyle: specification.style || ({})
    readonly property int moduleMargin: moduleStyle.margin ?? 12
    readonly property int moduleRadius: moduleStyle.radius ?? Theme.radius
    readonly property int moduleFontSize: moduleStyle.fontSize ?? 13
    readonly property color moduleBackground: resolveColor("background", Theme.background)
    readonly property color moduleForeground: resolveColor("foreground", Theme.text)
    readonly property color moduleAccent: resolveColor("accent", Theme.accent)
    readonly property var resolvedStyle: Object.assign({}, moduleStyle, {
        background: String(moduleBackground),
        foreground: String(moduleForeground),
        accent: String(moduleAccent)
    })

    data: [
        CelestialBackdrop {
            anchors.fill: parent
            accent: host.moduleAccent
            strength: 0.0
            visible: false
            z: -1
        },
        ExtensionSlot {
            id: extension
            anchors.fill: parent
            shell: host.shell
            target: host.extensionTarget
            context: host.extensionContext
            forceBuiltin: host.moduleId === "settings" && (host.shell.settingsPage === "modules" || host.shell.settingsPage === "plugins")
        }
    ]
    function resolveColor(key, fallback) {
        const value = moduleStyle[key];
        return !value || value === "inherit" ? fallback : value;
    }
    function moduleWidth(fallback) {
        return Math.max(1, Math.min(moduleStyle.width || fallback, screen ? screen.width - 2 * moduleMargin : 3840));
    }
    function moduleHeight(fallback) {
        return Math.max(1, Math.min(moduleStyle.height || fallback, screen ? screen.height - 2 * moduleMargin : 2160));
    }
    opened: true
    visible: (specification.enabled ?? true) && (opened || reveal > 0)
}
