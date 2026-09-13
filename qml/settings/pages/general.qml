import QtQuick
import QtQuick.Layouts
import "../components"
import "../../components"
import "../../style"
ColumnLayout {
    id: page
    required property var shell
    property bool confirmReset: false
    spacing: 18
    PageTitle { shell: page.shell; title: "General" }
    HelpText { shell: page.shell; message: "Interface language" }
    RowLayout {
        ShellButton { text: "English"; active: shell.state.language === "en_US"; onClicked: shell.command("language", "en_US") }
        ShellButton { text: shell.tr("Traditional Chinese"); active: shell.state.language === "zh_TW"; onClicked: shell.command("language", "zh_TW") }
    }
    HelpText { shell: page.shell; message: "Shell font" }
    RowLayout {
        Repeater {
            model: ["sans-serif", "serif", "monospace"]
            ShellButton { required property string modelData; text: modelData; active: (shell.state.appearance || {}).fontFamily === modelData; onClicked: shell.setAppearance({fontFamily: modelData}) }
        }
    }
    ShellButton { text: shell.tr("24-hour clock"); active: (shell.state.appearance || {}).clock24Hour ?? true; onClicked: shell.setAppearance({clock24Hour: !((shell.state.appearance || {}).clock24Hour ?? true)}) }
    ShellButton { text: shell.tr("First-run guide"); onClicked: { shell.settingsOpen = false; shell.command("setup", "") } }
    HelpText { shell: page.shell; message: "Desktop preferences are saved automatically. Display size applies to the current nested session. Existing files and system connections are preserved." }
    ShellButton { text: shell.tr("Reset desktop preferences"); onClicked: page.confirmReset = !page.confirmReset }
    RowLayout {
        visible: page.confirmReset
        ShellButton { text: shell.tr("Restore defaults"); onClicked: { shell.command("reset-preferences", ""); page.confirmReset = false } }
        ShellButton { text: shell.tr("Cancel"); onClicked: page.confirmReset = false }
    }
}
