import QtQuick
import QtQuick.Layouts
import "../components"
import "../../components"
import "../components" as SettingsComponents
ColumnLayout {
    id: page
    required property var shell
    spacing: 16
    PageTitle { shell: page.shell; title: "Applications and startup" }
    SettingsComponents.SettingsCard {
        title: shell.tr("Default terminal")
        description: shell.tr("Choose an installed desktop application for terminal sessions.")
        DefaultAppEditor { shell: page.shell; role: "terminal"; title: ""; Layout.fillWidth: true }
    }
    SettingsComponents.SettingsCard {
        title: shell.tr("Default file manager")
        description: shell.tr("Choose an installed desktop application for browsing files.")
        DefaultAppEditor { shell: page.shell; role: "files"; title: ""; Layout.fillWidth: true }
    }
    SettingsComponents.SettingsCard {
        title: shell.tr("Plugins")
        description: shell.tr("Manage installed QML plugins and C++ effects. The plugin store is reserved for a future release.")
        RowLayout {
            Layout.fillWidth: true
            ColumnLayout {
                Layout.fillWidth: true
                Text { text: shell.tr("Installed plugins") + ": " + (((shell.state.appearance || {}).plugins || []).length); color: Theme.text; font.family: Theme.font }
                HelpText { shell: page.shell; message: "QML plugins can update with the shell; C++ effects require a session restart after changing their state." }
            }
            ShellButton { text: shell.tr("Open plugin manager"); active: true; onClicked: shell.launch("plugins") }
        }
    }
    SettingsComponents.SettingsCard {
        title: shell.tr("LunaDash startup")
        description: shell.tr("Choose built-in tools to start with the next session.")
        Flow {
            Layout.fillWidth: true; spacing: 8
            Repeater {
                model: ["files", "console", "monitor", "welcome"]
                ShellButton {
                    required property string modelData
                    text: modelData; active: ((shell.state.appearance || {}).startupApps || []).includes(modelData)
                    onClicked: { const apps = ((shell.state.appearance || {}).startupApps || []).slice(); const index = apps.indexOf(modelData); if (index >= 0) apps.splice(index, 1); else apps.push(modelData); shell.setAppearance({startupApps: apps}) }
                }
            }
        }
    }
    HelpText { shell: page.shell; message: "Startup selection currently covers LunaDash tools. General desktop-entry autostart and session restoration remain JSON-only until implemented." }
}
