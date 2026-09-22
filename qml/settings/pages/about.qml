import QtQuick
import QtQuick.Layouts
import "../components"
import "../components" as SettingsComponents
import "../../components"
import "../../style"

ColumnLayout {
    id: page
    required property var shell
    readonly property var system: shell.state.system || ({})
    readonly property var update: shell.state.update || ({})
    spacing: 16

    PageTitle { shell: page.shell; title: "About LunaDash" }

    SettingsComponents.SettingsCard {
        title: ""
        RowLayout {
            Layout.fillWidth: true; spacing: 22
            LunaDashLogo { Layout.preferredWidth: 126; Layout.preferredHeight: 126; animated: Theme.animations }
            ColumnLayout {
                Layout.fillWidth: true; spacing: 6
                Text { text: "LunaDash " + (page.update.currentVersion || "1.0.1a"); color: Theme.text; font.family: Theme.font; font.pixelSize: 28; font.bold: true }
                Text { text: "⑨ baka ᗜˬᗜ"; color: Theme.muted; font.family: Theme.font; font.pixelSize: 13 }
                Text { visible: Boolean(page.update.currentCommit); text: String(page.update.currentCommit || "").slice(0, 12); color: Theme.muted; font.family: Theme.font; font.pixelSize: 10 }
            }
        }
    }

    SettingsComponents.UpdateCard { shell: page.shell }

    SettingsComponents.SettingsCard {
        title: shell.tr("Community and source")
        description: shell.tr("Follow development, report issues, and review the source code.")
        ShellButton { iconName: "github"; text: "GitHub"; onClicked: shell.openUrl(page.update.repositoryUrl || "https://github.com/LuYishan-4/LunaDash") }
    }

    SettingsComponents.SettingsCard {
        title: shell.tr("System information")
        GridLayout {
            Layout.fillWidth: true; columns: 2
            Text { text: shell.tr("Operating system"); color: Theme.muted; font.family: Theme.font } Text { wrapMode: Text.Wrap; Layout.minimumWidth: 0; Layout.fillWidth: true; text: page.system.os || "Linux"; color: Theme.text; font.family: Theme.font }
            Text { text: shell.tr("Kernel and architecture"); color: Theme.muted; font.family: Theme.font } Text { wrapMode: Text.Wrap; Layout.minimumWidth: 0; Layout.fillWidth: true; text: (page.system.kernel || "—") + " · " + (page.system.architecture || "—"); color: Theme.text; font.family: Theme.font }
            Text { text: shell.tr("Graphics API"); color: Theme.muted; font.family: Theme.font } Text { wrapMode: Text.Wrap; Layout.minimumWidth: 0; Layout.fillWidth: true; text: (shell.state.graphicsApi || "OpenGL") + " " + (shell.state.graphicsMajor || 0) + "." + (shell.state.graphicsMinor || 0); color: Theme.text; font.family: Theme.font }
        }
    }
}
