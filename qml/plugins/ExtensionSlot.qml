import QtQuick

// One visual extension point. Plugins receive the host's context before their
// required properties are evaluated. Failed replacements leave the original.
Item {
    id: slot
    property var shell: null
    required property string target
    property var context: ({})
    property bool forceBuiltin: false
    default property alias builtinData: builtin.data
    readonly property alias builtinItem: builtin
    property var descriptors: []
    property string fingerprint: ""
    property int readinessEpoch: 0
    readonly property var targetSpec: ((shell?.state?.extensions || {}).targets || []).find(entry => entry.id === target) || ({})
    readonly property var candidates: forceBuiltin ? [] : ((shell?.state?.extensions || {}).installed || []).filter(entry => entry.target === target && entry.enabled && entry.available && !entry.error && entry.type !== "effect")
    readonly property bool replacementReady: {
        const epoch = readinessEpoch;
        for (let i = 0; i < instances.count; ++i) {
            const instance = instances.itemAt(i);
            if (instance && instance.ready && instance.modelData.mode === "replace")
                return true;
        }
        return false;
    }
    readonly property bool builtinVisible: {
        const epoch = readinessEpoch;
        for (let i = 0; i < instances.count; ++i) {
            const instance = instances.itemAt(i);
            if (instance && instance.ready && instance.modelData.mode === "replace" && instance.modelData.type !== "opengl")
                return false;
        }
        return true;
    }
    function synchronize() {
        const selected = candidates.slice();
        const next = selected.sort((a, b) => a.mode === b.mode ? a.id.localeCompare(b.id) : a.mode === "replace" ? -1 : 1);
        const key = JSON.stringify(next);
        if (key === fingerprint)
            return;
        fingerprint = key;
        descriptors = next;
    }
    readonly property real pluginImplicitHeight: {
        const epoch = readinessEpoch;
        let height = 0;
        for (let i = 0; i < instances.count; ++i) {
            const instance = instances.itemAt(i);
            if (instance && instance.ready)
                height = Math.max(height, instance.contentHeight);
        }
        return height;
    }
    onCandidatesChanged: synchronize()
    Component.onCompleted: synchronize()

    data: [
        Item {
            id: builtin
            anchors.fill: parent
            visible: slot.builtinVisible
            enabled: visible
            opacity: slot.targetSpec.builtinSettings?.opacity ?? 1
        },
        Repeater {
            id: instances
            model: slot.descriptors
            delegate: Item {
                id: instance
                required property var modelData
                anchors.fill: parent
                visible: modelData.type === "opengl" || ready
                readonly property real contentHeight: loader.item ? loader.item.implicitHeight : 0
                readonly property bool ready: loader.status === Loader.Ready && loader.item && (loader.item.pluginReady ?? true)
                onReadyChanged: slot.readinessEpoch += 1
                Component.onDestruction: slot.readinessEpoch += 1
                readonly property var pluginContext: Object.assign({}, slot.context, {
                    target: slot.target,
                    source: builtin,
                    builtinSettings: slot.targetSpec.builtinSettings || {},
                    mode: modelData.mode,
                    reportError: message => slot.shell.command("extension-error", JSON.stringify({
                            id: modelData.instanceId || (modelData.id + "@" + modelData.target),
                            error: String(message)
                        }))
                })
                Loader {
                    id: loader
                    anchors.fill: parent
                    Component.onCompleted: {
                        const properties = {
                            shell: slot.shell
                        };
                        if (instance.modelData.schemaVersion === 2) {
                            properties.settings = instance.modelData.settings || {};
                            properties.context = Qt.binding(() => instance.pluginContext);
                        }
                        if (instance.modelData.type === "opengl") {
                            properties.shaders = instance.modelData.shaders;
                            setSource(Qt.resolvedUrl("ShaderPlugin.qml"), properties);
                        } else {
                            setSource(instance.modelData.entry, properties);
                        }
                    }
                    onStatusChanged: if (status === Loader.Error)
                        instance.pluginContext.reportError("Could not load plugin entry; the built-in remains available.")
                }
            }
        }
    ]
}
