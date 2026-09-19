import QtQuick

Item {
    id: host
    required property var shell
    width: 0
    height: 0

    readonly property var qmlPlugins: ((shell.state.appearance || {}).plugins || []).filter(plugin =>
        plugin.enabled && plugin.type === "qml" && plugin.entry)

    Repeater {
        model: host.qmlPlugins
        Loader {
            required property var modelData
            active: true
            source: modelData.entry
            onLoaded: {
                if (item && item.shell !== undefined)
                    item.shell = host.shell
            }
            onStatusChanged: {
                if (status === Loader.Error)
                    console.warn("Failed to load plugin " + modelData.id + " from " + modelData.entry)
            }
        }
    }
}
