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
    property bool confirmReset: false
    readonly property var languages: [
        {code: "en_US", name: "English"},
        {code: "zh_TW", name: "Traditional Chinese"}
    ]
    spacing: 16

    PageTitle { shell: page.shell; title: "General" }

    SettingsComponents.SettingsCard {
        title: shell.tr("Language and region")
        description: shell.tr("Choose the interface language. More language packs can be added later without changing this layout.")
        RowLayout {
            Layout.fillWidth: true
            Text { text: shell.tr("Interface language"); color: Theme.text; Layout.fillWidth: true }
            StyledComboBox {
                id: language
                model: page.languages
                textRole: "name"
                valueRole: "code"
                currentIndex: Math.max(0, page.languages.findIndex(item => item.code === shell.state.language))
                onActivated: shell.command("language", currentValue)
                Accessible.name: shell.tr("Interface language")
            }
        }
    }

    SettingsComponents.SettingsCard {
        title: shell.tr("Typography and time")
        description: shell.tr("Adjust the shell typeface and clock format.")
        RowLayout {
            Layout.fillWidth: true
            Text { text: shell.tr("Shell font"); color: Theme.text; Layout.fillWidth: true }
            StyledComboBox {
                model: ["sans-serif", "serif", "monospace"]
                currentIndex: model.indexOf((shell.state.appearance || {}).fontFamily || "sans-serif")
                onActivated: shell.setAppearance({fontFamily: currentText})
            }
        }
        ShellButton {
            text: shell.tr("24-hour clock")
            active: (shell.state.appearance || {}).clock24Hour ?? true
            onClicked: shell.setAppearance({clock24Hour: !((shell.state.appearance || {}).clock24Hour ?? true)})
        }
    }

    SettingsComponents.SettingsCard {
        title: shell.tr("Setup and recovery")
        description: shell.tr("Preferences are saved automatically. Resetting does not remove personal files or network profiles.")
        RowLayout {
            ShellButton { text: shell.tr("First-run guide"); onClicked: { shell.settingsOpen = false; shell.command("setup", "") } }
            ShellButton { text: shell.tr("Reset desktop preferences"); onClicked: page.confirmReset = !page.confirmReset }
        }
        RowLayout {
            visible: page.confirmReset
            ShellButton { text: shell.tr("Restore defaults"); onClicked: { shell.command("reset-preferences", ""); page.confirmReset = false } }
            ShellButton { text: shell.tr("Cancel"); onClicked: page.confirmReset = false }
        }
    }
}
