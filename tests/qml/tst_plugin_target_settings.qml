import QtQuick
import QtTest
import "../../qml/plugins"

TestCase {
    id: test
    name: "PluginTargetSettings"
    when: windowShown
    width: 800
    height: 800

    QtObject {
        id: fakeShell
        property var state: ({
            extensions: {
                targets: [
                    {id: "window-animation", name: "Window animation", category: "Animation", types: ["effect"]},
                    {id: "panel", name: "Taskbar", category: "Desktop", types: ["quickshell", "opengl"]}
                ],
                installed: [
                    {id: "org.test.fade", name: "Fade", target: "window-animation", type: "effect", enabled: true, status: "available"},
                    {id: "org.test.panel", name: "Panel replacement", target: "panel", type: "quickshell", enabled: false, status: "disabled"},
                    {id: "org.test.shader", name: "Panel shader", target: "panel", type: "opengl", enabled: true, status: "available"}
                ]
            },
            settingsApi: {
                targets: [
                    {id: "builtin:window-animation", name: "Window animation", type: "builtin", schema: {duration: {type: "integer", default: 200}}, values: {duration: 200}, revision: "a"},
                    {id: "builtin:panel", name: "Taskbar", type: "builtin", schema: {opacity: {type: "number", default: 1}}, values: {opacity: 1}, revision: "b"},
                    {id: "plugin:org.test.fade", name: "Fade", type: "effect", schema: {duration: {type: "integer", default: 260}}, values: {duration: 260}, revision: "c"},
                    {id: "plugin:org.test.panel", name: "Panel replacement", type: "quickshell", schema: {}, values: {}, revision: "d"},
                    {id: "plugin:org.test.shader", name: "Panel shader", type: "opengl", schema: {strength: {type: "number", default: 0.5}}, values: {strength: 0.5}, revision: "e"}
                ]
            }
        })
        signal commandCompleted(string method, var result)
        function tr(text) { return text }
        function command(method, value) {}
    }

    TargetSettings {
        id: settings
        width: 760
        shell: fakeShell
    }

    function test_type_filter_groups_by_target() {
        compare(settings.visibleTargets.length, 2)
        compare(settings.pluginsForTarget("panel").length, 2)
        settings.selectedType = "effect"
        compare(settings.visibleTargets.length, 1)
        compare(settings.visibleTargets[0].id, "window-animation")
        compare(settings.pluginsForTarget("window-animation").length, 1)
        settings.selectedType = "quickshell"
        compare(settings.visibleTargets.length, 1)
        compare(settings.visibleTargets[0].id, "panel")
        compare(settings.pluginsForTarget("panel").length, 1)
    }

    function test_target_entries_always_start_with_native() {
        settings.selectedType = "all"
        const panel = settings.targets.find(target => target.id === "panel")
        const entries = settings.entriesForTarget(panel)
        compare(entries.length, 3)
        compare(entries[0].kind, "native")
        compare(entries[0].settingsId, "builtin:panel")
        compare(entries[1].kind, "plugin")
    }

    function test_plugin_parameters_resolve_through_settings_api() {
        verify(settings.hasSettings("plugin:org.test.fade"))
        verify(settings.hasSettings("plugin:org.test.shader"))
        verify(!settings.hasSettings("plugin:org.test.panel"))
    }
}
