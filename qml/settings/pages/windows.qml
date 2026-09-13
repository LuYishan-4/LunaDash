import QtQuick
import QtQuick.Layouts
import "../components"
import "../../components"
ColumnLayout {
    id: page
    required property var shell
    spacing: 20
    PageTitle { shell: page.shell; title: "Windows and workspaces" }
    PreferenceSlider { shell: page.shell; preference: "workspaceCount"; label: "Number of workspaces"; minimum: 1; maximum: 9 }
    PreferenceSlider { shell: page.shell; preference: "masterRatio"; label: "Master window width"; minimum: 30; maximum: 70; suffix: "%" }
    PreferenceSlider { shell: page.shell; preference: "gap"; label: "Window gaps"; minimum: 4; maximum: 32; suffix: " px" }
    ShellButton { text: shell.tr("Open new windows floating"); active: (shell.state.appearance || {}).defaultFloating ?? false; onClicked: shell.setAppearance({defaultFloating: !((shell.state.appearance || {}).defaultFloating ?? false)}) }
    HelpText { shell: page.shell; message: "Reducing the workspace count moves windows from removed workspaces to the last remaining workspace." }
    HelpText { shell: page.shell; message: "Super + 1–9 switches workspaces. Add Shift to move a window. Super + Space toggles floating; Super + H / L adjusts the master width." }
}
