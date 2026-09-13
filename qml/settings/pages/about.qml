import QtQuick
import QtQuick.Layouts
import "../components"
import "../../style"
ColumnLayout {
    id: page
    required property var shell
    spacing: 18
    PageTitle { shell: page.shell; title: "About LuDash" }
    Text { text: "LuDash 0.1  /  Wayland"; color: Theme.text; font.pixelSize: 23 }
    Text { text: (shell.state.system || {}).os || ""; color: Theme.text }
    Text { text: ((shell.state.system || {}).kernel || "") + "  ·  " + ((shell.state.system || {}).architecture || ""); color: Theme.muted }
    Text { text: (shell.state.graphicsApi || "") + " " + (shell.state.graphicsMajor || 0) + "." + (shell.state.graphicsMinor || 0); color: Theme.accent }
    HelpText { shell: page.shell; message: "C11 rendering and data cores, C++20 Wayland integration, and a Quickshell interface." }
    HelpText { shell: page.shell; message: "This is a development preview. Available settings depend on the implemented compositor features, installed tools and running system services." }
    HelpText { shell: page.shell; message: "English documentation: docs/SETTINGS.md and docs/TESTING_AND_FILES.md in the source tree." }
}
