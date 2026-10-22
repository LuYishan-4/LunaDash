import QtQuick
import QtQuick.Layouts
import "../components"
import "../components" as SettingsComponents
import "../../components"
import "../../style"

ColumnLayout {
    id: page
    required property var shell
    readonly property var appearance: shell.state.appearance || ({})
    spacing: 16

    PageTitle { shell: page.shell; title: "Privacy and accessibility" }

    SettingsComponents.SettingsCard {
        title: shell.tr("Identity and accessibility")
        description: shell.tr("Control what LunaDash exposes in its own UI and reduce visual motion without changing application content.")

        RowLayout {
            Layout.fillWidth: true
            Text { Layout.fillWidth: true; text: shell.tr("Show user and host"); color: Theme.text; font.family: Theme.font }
            SoftSwitch {
                checked: Boolean(page.appearance.showHostDetails ?? false)
                onToggled: shell.setAppearance({showHostDetails: checked})
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Text { Layout.fillWidth: true; text: shell.tr("Reduced motion"); color: Theme.text; font.family: Theme.font }
            SoftSwitch {
                checked: !Boolean(page.appearance.animations ?? true)
                onToggled: shell.setAppearance({animations: !checked})
            }
        }
    }

    SettingsComponents.SettingsCard {
        title: shell.tr("Plugin trust")
        description: shell.tr("Native compositor plugins run with the desktop process. Only enable packages you trust.")
        ShellButton {
            text: shell.tr("Open plugin settings")
            active: true
            onClicked: shell.command("open-settings", "plugins")
        }
    }

    SettingsComponents.SettingsCard {
        title: shell.tr("System security")
        description: shell.tr("Screen locking, accessibility services and privileged host controls are delegated to supported system services when available.")
        ToolList { shell: page.shell; category: "privacy" }
    }
}
