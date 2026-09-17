import QtQuick
import "../components"
import "../style"
AnimatedPanel {
    id: host
    required property string moduleId
    required property var shell
    default property alias moduleData: builtin.data
    readonly property var specification: ((shell.state.shellModules || {}).modules || {})[moduleId] || ({})
    readonly property var moduleStyle: specification.style || ({})
    readonly property int moduleMargin: moduleStyle.margin ?? 12
    readonly property int moduleRadius: moduleStyle.radius ?? Theme.radius
    readonly property int moduleFontSize: moduleStyle.fontSize ?? 13
    readonly property color moduleBackground: resolveColor("background", Theme.background)
    readonly property color moduleForeground: resolveColor("foreground", Theme.text)
    readonly property color moduleAccent: resolveColor("accent", Theme.accent)
    readonly property var resolvedStyle: Object.assign({}, moduleStyle, {background: String(moduleBackground), foreground: String(moduleForeground), accent: String(moduleAccent)})

    CelestialBackdrop {
        anchors.fill: parent
        accent: host.moduleAccent
        strength: 0.9
        z: -1
    }

    Item { id: builtin; anchors.fill: parent }
    function resolveColor(key, fallback) { const value = moduleStyle[key]; return !value || value === "inherit" ? fallback : value }
    function moduleWidth(fallback) { return Math.max(1, Math.min(moduleStyle.width || fallback, screen ? screen.width - 2 * moduleMargin : 3840)) }
    function moduleHeight(fallback) { return Math.max(1, Math.min(moduleStyle.height || fallback, screen ? screen.height - 2 * moduleMargin : 2160)) }
    opened: true
    visible: (specification.enabled ?? true) && (opened || reveal > 0)
}
