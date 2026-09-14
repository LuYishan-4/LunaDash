import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components"
import "../components" as SettingsComponents
import "../../components"
import "../../style"

ColumnLayout {
    id: page
    required property var shell
    spacing: 16

    readonly property var actions: {
        let result = [
            {id:"focusLeft", name:"Focus column left"}, {id:"focusRight", name:"Focus column right"},
            {id:"focusUp", name:"Focus window up"}, {id:"focusDown", name:"Focus window down"},
            {id:"groupLeft", name:"Group with left column"}, {id:"groupRight", name:"Group with right column"},
            {id:"reorderLeft", name:"Move column left"}, {id:"reorderRight", name:"Move column right"},
            {id:"expelWindow", name:"Expel window from group"}, {id:"centerColumn", name:"Center column"},
            {id:"widenColumn", name:"Widen column"}, {id:"narrowColumn", name:"Narrow column"},
            {id:"maximizeWindow", name:"Maximize or restore window"}, {id:"closeWindow", name:"Close window"},
            {id:"closeWindowAlternate", name:"Close window alternative"}, {id:"minimizeWindow", name:"Minimize window"},
            {id:"toggleFloating", name:"Toggle floating"}, {id:"launchTerminal", name:"Open terminal"},
            {id:"launchFiles", name:"Open files"}, {id:"launchLauncher", name:"Open launcher"}
        ]
        for (let workspace = 1; workspace <= 9; ++workspace) {
            result.push({id:"workspace" + workspace, name:"Switch to workspace " + workspace})
            result.push({id:"moveToWorkspace" + workspace, name:"Move window to workspace " + workspace})
        }
        return result
    }

    function save(action, sequence) {
        shell.command("shortcuts", JSON.stringify({[action]: sequence}))
    }

    PageTitle { shell: page.shell; title: "Keyboard shortcuts" }
    SettingsComponents.SettingsCard {
        title: shell.tr("Shortcut policy")
        description: shell.tr("Every global shortcut uses the Meta key. Duplicate or invalid combinations are rejected before they are saved.")
        RowLayout {
            Layout.fillWidth: true
            Text { text: shell.tr("Changes apply immediately"); color: Theme.muted; Layout.fillWidth: true }
            ShellButton { text: shell.tr("Restore shortcut defaults"); onClicked: shell.command("reset-shortcuts", "") }
        }
    }

    Repeater {
        model: page.actions
        SettingsComponents.SettingsCard {
            required property var modelData
            title: shell.tr(modelData.name)
            RowLayout {
                Layout.fillWidth: true
                Text { text: modelData.id; color: Theme.muted; font.family: "monospace"; Layout.fillWidth: true }
                SettingsComponents.ShortcutRecorder {
                    shell: page.shell
                    sequence: String((shell.state.shortcuts || {})[modelData.id] || "Disabled")
                    onAccepted: sequence => page.save(modelData.id, sequence)
                    Accessible.name: shell.tr(modelData.name)
                }
            }
        }
    }
}
