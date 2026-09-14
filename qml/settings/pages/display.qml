import QtQuick
import QtQuick.Layouts
import "../components"
import "../../components"
import "../../style"
ColumnLayout {
    id: page
    required property var shell
    property var output: shell.state.display || ({})
    spacing: 20
    PageTitle { shell: page.shell; title: "Display" }
    Text { text: (page.output.output || "") + "  ·  " + (page.output.width || 0) + " × " + (page.output.height || 0); color: Theme.text; font.pixelSize: 18 }
    Text { text: "Scale  " + (page.output.scale || 1) + "×   /   " + Math.round(page.output.refreshRate || 0) + " Hz"; color: Theme.muted }
    HelpText { shell: page.shell; message: "Nested desktop size" }
    RowLayout {
        Repeater {
            model: ["1280x720", "1440x900", "1920x1080"]
            ShellButton { required property string modelData; text: modelData; enabled: page.output.nested && !page.output.fullscreen; onClicked: shell.command("desktop-size", modelData) }
        }
    }
    HelpText { shell: page.shell; message: "In a nested session, physical monitor resolution, refresh rate, scaling, rotation and night light are controlled by your host desktop. These buttons resize only LunaDah's window." }
    HelpText { shell: page.shell; message: "Standalone multi-monitor configuration, HDR, color profiles and night light are not available yet." }
    ToolList { shell: page.shell; category: "display" }
}
