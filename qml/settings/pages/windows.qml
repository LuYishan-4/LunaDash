import QtQuick
import QtQuick.Layouts
import "../components"
import "../../components"
ColumnLayout {
    id: page
    required property var shell
    spacing: 16
    PageTitle { shell: page.shell; title: "Windows and workspaces" }
    PreferenceSlider { shell: page.shell; preference: "workspaceCount"; label: "Number of workspaces"; minimum: 1; maximum: 10 }
    PreferenceSlider { shell: page.shell; preference: "masterRatio"; label: "Default column width"; minimum: 30; maximum: 70; suffix: "%" }
    PreferenceSlider { shell: page.shell; preference: "gap"; label: "Window gaps"; minimum: 4; maximum: 32; suffix: " px" }
    HelpText { shell: page.shell; message: "New windows open in separate columns. Each column can hold up to eight vertically tiled windows. Dialogs stay above their parent window." }
    HelpText { shell: page.shell; message: "Alt + left-drag swaps window slots. Drop near the top or bottom edge to insert into a column. With only one window, Alt dragging moves it without resizing." }
    HelpText { shell: page.shell; message: "Shift + Alt + left-drag resizes a window and redistributes space in its column without changing the window order." }
    HelpText { shell: page.shell; message: "The taskbar groups windows by workspace in capsules. The current workspace is brighter; other workspaces are darker. Select a tiled task to maximize it and hide its workspace peers. Click it again or press Super + F to restore the saved tiling layout." }
    HelpText { shell: page.shell; message: "Alt + Tab shows workspaces 1–10 in a two-row overview with window thumbnails. Use Tab, arrow keys or scrolling; release Alt to switch workspace, or press Esc to cancel." }
    HelpText { shell: page.shell; message: "Super + 1–9 switches workspaces; Super + 0 selects workspace 10. Add Shift to move a window. Super + H / L focuses columns; Super + J / K focuses rows. Super + F maximizes or restores the focused window." }
    HelpText { shell: page.shell; message: "Reducing the workspace count moves windows from removed workspaces to the last remaining workspace." }
}
