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
            {id:"focusLeft", name:"Focus column left"},
            {id:"focusRight", name:"Focus column right"},
            {id:"focusUp", name:"Focus window up"},
            {id:"focusDown", name:"Focus window down"},
            {id:"groupLeft", name:"Group with left column"},
            {id:"groupRight", name:"Group with right column"},
            {id:"reorderLeft", name:"Move column left"},
            {id:"reorderRight", name:"Move column right"},
            {id:"expelWindow", name:"Expel window from group"},
            {id:"centerColumn", name:"Center column"},
            {id:"widenColumn", name:"Widen column"},
            {id:"narrowColumn", name:"Narrow column"},
            {id:"maximizeWindow", name:"Maximize or restore window"},
            {id:"closeWindow", name:"Close window"},
            {id:"closeWindowAlternate", name:"Close window alternative"},
            {id:"minimizeWindow", name:"Minimize window"},
            {id:"launchTerminal", name:"Open terminal"},
            {id:"launchTerminalAlternate", name:"Open terminal alternative"},
            {id:"launchFiles", name:"Open files"},
            {id:"launchLauncher", name:"Open launcher"},
            {id:"screenshot", name:"Take a screenshot"},
            {id:"launchOrbit", name:"Orbit launcher"},
            {id:"chooseWallpaper", name:"Wallpaper library"},
            {id:"randomWallpaper", name:"Random wallpaper"},
            {id:"toggleEyeCare", name:"Eye care"},
            {id:"toggleScratchpad", name:"Scratchpad"},
            {id:"toggleFullscreen", name:"Toggle fullscreen"},
            {id:"openControlCenter", name:"Control center"},
            {id:"openClipboard", name:"Clipboard"},
            {id:"openPowerMenu", name:"Session controls"}
        ]
        for (let workspace = 1; workspace <= 10; ++workspace) {
            result.push({id:"workspace" + workspace, name:"Switch to workspace %1", workspace:workspace})
            result.push({id:"moveToWorkspace" + workspace, name:"Move window to workspace %1", workspace:workspace})
        }
        const available = shell.state.shortcuts || ({})
        return result.filter(action =>
            Object.prototype.hasOwnProperty.call(available, action.id))
    }
    function actionLabel(action) {
        const label = shell.tr(action.name)
        return action.workspace ? label.arg(action.workspace) : label
    }
    function save(action, sequence) { shell.command("shortcuts", JSON.stringify({[action]:sequence})) }

    PageTitle { shell: page.shell; title: "Keyboard shortcuts" }
    SettingsComponents.SettingsCard {
        title: shell.tr("Shortcut policy")
        description: shell.tr("Every global shortcut uses Meta or Alt. Duplicate or invalid combinations are rejected before they are saved.")
        RowLayout {
            Layout.fillWidth: true
            Text { wrapMode: Text.Wrap; Layout.minimumWidth: 0; text: shell.tr("Changes apply immediately"); color: Theme.muted; font.family: Theme.font; Layout.fillWidth: true }
            ShellButton { text: shell.tr("Restore shortcut defaults"); onClicked: shell.command("reset-shortcuts", "") }
        }
    }
    Repeater {
        model: page.actions
        SettingsComponents.SettingsCard {
            required property var modelData
            title: page.actionLabel(modelData)
            RowLayout {
                Layout.fillWidth: true
                Text { wrapMode: Text.Wrap; Layout.minimumWidth: 0; text: modelData.id; color: Theme.muted; font.family: Theme.font; Layout.fillWidth: true }
                SettingsComponents.ShortcutRecorder {
                    shell: page.shell
                    sequence: String((shell.state.shortcuts || {})[modelData.id] || "Disabled")
                    onAccepted: sequence => page.save(modelData.id, sequence)
                    Accessible.name: page.actionLabel(modelData)
                }
            }
        }
    }
    SettingsComponents.SettingsCard {
        title: shell.tr("Fixed desktop shortcuts")
        description: shell.tr("These built-in controls are separate from the configurable shortcuts above.")
        Repeater {
            model: [
                {keys:"Meta", label:"Tap Meta to open the launcher"},
                {keys:"F12", label:"Toggle fullscreen"},
                {keys:"Alt+Tab / Alt+Shift+Tab", label:"Window switcher"},
                {keys:"Super+Tab / Super+Shift+Tab", label:"Workspace switcher"},
                {keys:"Drag titlebar", label:"Drag a window onto another to swap them"},
                {keys:"Alt+Shift + drag", label:"Resize a window"}
            ]
            RowLayout {
                required property var modelData
                Layout.fillWidth: true
                Text { Layout.fillWidth: true; Layout.minimumWidth: 0; wrapMode: Text.Wrap; text: shell.tr(modelData.label); color: Theme.text; font.family: Theme.font }
                Text { Layout.maximumWidth: 240; wrapMode: Text.Wrap; text: modelData.keys; color: Theme.muted; font.family: Theme.font }
            }
        }
        HelpText { shell: page.shell; message: "Window and workspace switchers accept arrow keys; Enter or releasing Alt/Super confirms, and Escape cancels." }
    }
}
