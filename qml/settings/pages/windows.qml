import QtQuick
import QtQuick.Layouts
import "../components"
import "../../components"
import "../../plugins"
ColumnLayout {
    id: page
    required property var shell
    spacing: 16
    PageTitle { shell: page.shell; title: "Windows and workspaces" }
    PreferenceSlider { shell: page.shell; preference: "workspaceCount"; label: "Number of workspaces"; minimum: 1; maximum: 10 }
    readonly property var layoutApi: page.shell.state.windowLayout || ({})
    SettingsCard {
        Layout.fillWidth: true
        title: page.shell.tr("Window layout")
        description: page.shell.tr("Active template:") + " " + String(page.layoutApi.template || "tiling")
        ExtensionOptions {
            Layout.fillWidth: true
            shell: page.shell
            schema: page.layoutApi.schema || ({})
            values: page.layoutApi.values || ({})
            onEdited: (key, value) => {
                const changes = ({})
                changes[key] = value
                page.shell.command("window-layout-settings", JSON.stringify(changes))
            }
        }
    }
    HelpText { shell: page.shell; message: "Windows stay inside one screen. Two windows split left and right; later windows split the largest tile. Groups can hold up to eight vertical rows. Dialogs stay above their parent window." }
    HelpText { shell: page.shell; message: "Alt + left-drag swaps window slots. Drop near the top or bottom edge to insert into a column. With only one window, Alt dragging moves it without resizing." }
    HelpText { shell: page.shell; message: "Shift + Alt + left-drag moves shared tile boundaries or resizes grouped rows. Neighbors share the available space without overlapping or changing order." }
    HelpText { shell: page.shell; message: "The taskbar groups windows by workspace in capsules. The current workspace is brighter; other workspaces are darker. Select a tiled task to maximize it and hide its workspace peers. Click it again or press Super + F to restore the saved tiling layout." }
    HelpText { shell: page.shell; message: "Alt + Tab shows workspaces 1–10 in a two-row overview with window thumbnails. Use Tab, arrow keys or scrolling; release Alt to switch workspace, or press Esc to cancel." }
    HelpText { shell: page.shell; message: "Super + 1–9 switches workspaces; Super + 0 selects workspace 10. Add Shift to move a window. Super + H / J / K / L focuses left / down / up / right. Super + F maximizes one window or restores all tiles." }
    HelpText { shell: page.shell; message: "Reducing the workspace count moves windows from removed workspaces to the last remaining workspace." }
}
