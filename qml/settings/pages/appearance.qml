import QtQuick
import QtQuick.Layouts
import "../components"
import "../../components"
import "../../configuration"
import "../../effects"
import "../../wallpaper"
import "../../launcher"
import "../../style"

ColumnLayout {
    id: page
    required property var shell
    readonly property var moduleDocument: (shell.state.shellModules || {}).document || ({schemaVersion: 1, modules: {}})
    readonly property var panelConfig: ((moduleDocument.modules || {}).panel || {}).config || ({})
    spacing: 16

    function updatePanel(field, value) {
        const next = JSON.parse(JSON.stringify(page.moduleDocument))
        if (!next.modules || !next.modules.panel)
            return
        if (!next.modules.panel.config)
            next.modules.panel.config = {}
        next.modules.panel.config[field] = value
        shell.command("module-save", JSON.stringify(next))
    }

    PageTitle { shell: page.shell; title: "Appearance" }
    SettingsCard {
        title: shell.tr("Panel layout")
        description: shell.tr("Choose the panel contents. The centered launcher places the clock beside the status controls.")

        Repeater {
            model: [
                {key: "centerLauncher", label: "Centered launcher"},
                {key: "showActiveTitle", label: "Show active window title"},
                {key: "showSystemStats", label: "Show CPU and memory"},
                {key: "occupiedWorkspacesOnly", label: "Compact workspace list"}
            ]
            RowLayout {
                required property var modelData
                Layout.fillWidth: true
                Text {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    text: shell.tr(modelData.label)
                    color: Theme.text
                    font.family: Theme.font
                    wrapMode: Text.Wrap
                }
                SoftSwitch {
                    checked: page.panelConfig[modelData.key] ?? true
                    onToggled: page.updatePanel(modelData.key, checked)
                }
            }
        }
        Text {
            Layout.fillWidth: true
            text: shell.tr("Show occupied workspaces, the current workspace and one empty workspace. All configured workspaces remain available.")
            color: Theme.muted
            font.family: Theme.font
            wrapMode: Text.Wrap
        }
        ShellButton {
            text: shell.tr("More panel options")
            onClicked: shell.command("open-settings", "modules")
        }
    }
    WallpaperLibrary { shell: page.shell; Layout.fillWidth: true }
    ThemeControls { shell: page.shell; Layout.fillWidth: true }
    WidgetControls { shell: page.shell; Layout.fillWidth: true }
    AppearanceControls { shell: page.shell; Layout.fillWidth: true }
    EffectsControls { shell: page.shell; Layout.fillWidth: true }
    OrbitSettings { shell: page.shell; Layout.fillWidth: true }
}
