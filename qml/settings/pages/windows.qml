import QtQuick
import QtQuick.Layouts
import "../components"
import "../../components"
ColumnLayout {
    id: page
    required property var shell
    spacing: 16
    PageTitle { shell: page.shell; title: "Windows and workspaces" }
    PreferenceSlider { shell: page.shell; preference: "workspaceCount"; label: "Number of workspaces"; minimum: 1; maximum: 9 }
    PreferenceSlider { shell: page.shell; preference: "masterRatio"; label: "Default column width"; minimum: 30; maximum: 70; suffix: "%" }
    PreferenceSlider { shell: page.shell; preference: "gap"; label: "Window gaps"; minimum: 4; maximum: 32; suffix: " px" }
    HelpText { shell: page.shell; message: "Columns hold up to eight tiled windows. New windows join the focused column when it has room. Dialogs stay above their parent window." }
    HelpText { shell: page.shell; message: "Alt + left-drag swaps window slots. Drop near the top or bottom edge to insert into a column. With only one window, Alt dragging moves it without resizing." }
    HelpText { shell: page.shell; message: "Shift + Alt + left-drag resizes a window and redistributes space in its column without changing the window order." }
    HelpText { shell: page.shell; message: "The taskbar lists individual windows. Selecting one enlarges its column and gives it more height while keeping the other windows visible." }
    HelpText { shell: page.shell; message: "Alt + Tab opens the horizontal window selector. Use Tab, Shift + Tab, arrow keys or scrolling; release Alt to switch, or press Esc to cancel." }
    HelpText { shell: page.shell; message: "Super + 1–9 switches workspaces. Add Shift to move a window. Super + H / L focuses columns; Super + J / K focuses rows. Super + F maximizes or restores the column width." }
    HelpText { shell: page.shell; message: "Reducing the workspace count moves windows from removed workspaces to the last remaining workspace." }
}
