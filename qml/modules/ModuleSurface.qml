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
    readonly property string signature: ((specification.custom || {}).source || "") + ":" + ((shell.state.shellModules || {}).revision || 0)
    property bool initialized: false
    property bool customReady: false
    function resolveColor(key, fallback) { const value = moduleStyle[key]; return !value || value === "inherit" ? fallback : value }
    function moduleWidth(fallback) { return Math.max(1, Math.min(moduleStyle.width || fallback, screen ? screen.width - 2 * moduleMargin : 3840)) }
    function moduleHeight(fallback) { return Math.max(1, Math.min(moduleStyle.height || fallback, screen ? screen.height - 2 * moduleMargin : 2160)) }
    function loadCustom() {
        if (!initialized) return
        customReady = false
        custom.source = ""
        const source = (specification.custom || {}).source || ""
        if (source) custom.setSource(source + "?revision=" + ((shell.state.shellModules || {}).revision || 0), {shell: host.shell, style: host.resolvedStyle, moduleId: host.moduleId})
    }
    function loadFailed() { customReady = false; shell.command("module-error", JSON.stringify({id: moduleId, error: "Custom QML failed to load as an Item. Built-in content is active; see the Quickshell log."})) }
    onSignatureChanged: loadCustom()
    onResolvedStyleChanged: if (customReady && custom.item) custom.item.style = resolvedStyle
    opened: true
    visible: (specification.enabled ?? true) && (opened || reveal > 0)
    contentItem.data: [
        Item { id: builtin; anchors.fill: parent; visible: !host.customReady },
        Loader {
            id: custom; anchors.fill: parent; visible: host.customReady
            onStatusChanged: if (status === Loader.Error) host.loadFailed()
            onLoaded: { if (item instanceof Item) host.customReady = true; else host.loadFailed() }
        }
    ]
    Component.onCompleted: { initialized = true; loadCustom() }
}
