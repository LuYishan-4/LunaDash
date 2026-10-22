import QtQuick

Item {
    id: plugin
    required property var shell
    required property var settings
    required property var context
    required property var shaders
    readonly property bool supported: GraphicsInfo.api !== GraphicsInfo.Software && GraphicsInfo.api !== GraphicsInfo.Unknown
    readonly property bool pluginReady: supported && effect.status === ShaderEffect.Compiled
    ShaderEffectSource {
        id: capture
        sourceItem: plugin.context.source
        hideSource: plugin.pluginReady && plugin.context.mode === "replace"
        live: plugin.visible && plugin.supported
        visible: false
    }
    ShaderEffect {
        id: effect
        anchors.fill: parent
        visible: plugin.supported
        vertexShader: plugin.shaders.vertex || ""
        fragmentShader: plugin.shaders.fragment || ""
        property var source: capture
        property real strength: Number(plugin.settings.strength ?? 1)
        property vector2d resolution: Qt.vector2d(width, height)
        property vector4d parameters: Qt.vector4d(Number(plugin.settings.parameter0 ?? 0), Number(plugin.settings.parameter1 ?? 0), Number(plugin.settings.parameter2 ?? 0), Number(plugin.settings.parameter3 ?? 0))
        // Fixed uniforms keep shader packages portable; arbitrary visual logic
        // and additional inputs belong in a Quickshell plugin's own effects.
        onStatusChanged: if (status === ShaderEffect.Error)
            plugin.context.reportError(log || "Shader compilation failed")
    }
}
