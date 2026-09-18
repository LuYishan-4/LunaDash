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
    ShellButton { text: shell.tr("Open new windows floating"); active: (shell.state.appearance || {}).defaultFloating ?? false; onClicked: shell.setAppearance({defaultFloating: !((shell.state.appearance || {}).defaultFloating ?? false)}) }
    ShellButton {
        text: shell.tr("Alt + right-drag freely resizes windows")
        active: (shell.state.appearance || {}).altMouseResize ?? true
        onClicked: shell.setAppearance({altMouseResize: !((shell.state.appearance || {}).altMouseResize ?? true)})
    }
    HelpText { shell: page.shell; message: "While Alt resizing, LunaDash shows the original tiled guide. Shrinking one window leaves the others unchanged; growing it asks neighbouring tiled columns to yield space while preserving the configured gap. Drag back onto the guide to restore the tiled size." }
    HelpText { shell: page.shell; message: "New tiled windows open at the configured column width. Meta + F maximizes the focused column and restores its previous width when pressed again." }
    HelpText { shell: page.shell; message: "Reducing the workspace count moves windows from removed workspaces to the last remaining workspace." }
    HelpText { shell: page.shell; message: "Super + 1–9 switches workspaces. Add Shift to move a window. Super + H / L focuses columns; add Control to reorder them. Super + / - resizes the focused column, and Super + C centers it." }
    HelpText { shell: page.shell; message: "Grouping controls: Super + J / K moves between windows, including grouped members. In the column strip, drag one application icon onto another to group them; click a member to focus it, or right-click to expel it." }
}
