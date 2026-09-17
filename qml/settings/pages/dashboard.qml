import QtQuick
import QtQuick.Layouts
import "../components"
import "../../components"
import "../../style"

ColumnLayout {
    id: page
    required property var shell
    spacing: 16

    readonly property var shellModules: shell.state.shellModules || ({})
    readonly property var document: shellModules.document || ({schemaVersion:1, modules:{}})
    readonly property var overview: ((document.modules || {}).overview || ({}))
    readonly property var config: overview.config || ({})
    readonly property var shortcutChoices: [
        {id:"files", label:"Files"},
        {id:"terminal", label:"Terminal"},
        {id:"settings", label:"Desktop settings"},
        {id:"monitor", label:"System monitor"},
        {id:"network", label:"Network settings"},
        {id:"plugins", label:"Plugins"}
    ]

    function updateConfig(field, value) {
        const next = JSON.parse(JSON.stringify(page.document))
        if (!next.modules || !next.modules.overview)
            return
        if (!next.modules.overview.config)
            next.modules.overview.config = {}
        next.modules.overview.config[field] = value
        shell.command("module-save", JSON.stringify(next))
    }

    function setShortcut(id, enabled) {
        let shortcuts = (page.config.shortcuts || ["files", "terminal", "settings"]).slice()
        const index = shortcuts.indexOf(id)
        if (enabled && index < 0)
            shortcuts.push(id)
        else if (!enabled && index >= 0)
            shortcuts.splice(index, 1)
        updateConfig("shortcuts", shortcuts)
    }

    PageTitle { shell: page.shell; title: "Dashboard" }

    SettingsCard {
        title: shell.tr("Dashboard")
        description: shell.tr("Open the dashboard from the center panel button, then choose which quick controls and cards should appear.")

        RowLayout {
            Layout.fillWidth: true
            Text { text: shell.tr("Media player"); color: Theme.text; font.family: Theme.font; Layout.fillWidth: true }
            SoftSwitch {
                checked: page.config.showMedia ?? true
                onToggled: page.updateConfig("showMedia", checked)
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Text { text: shell.tr("Volume and Wi-Fi quick controls"); color: Theme.text; font.family: Theme.font; Layout.fillWidth: true }
            SoftSwitch {
                checked: page.config.quickControls ?? true
                onToggled: page.updateConfig("quickControls", checked)
            }
        }

        ShellButton {
            text: shell.tr("Open dashboard")
            active: true
            onClicked: shell.setAppearance({overview:true})
        }
    }

    SettingsCard {
        title: shell.tr("Quick launch")
        description: shell.tr("Choose up to six launchers shown on the dashboard. These values are stored in overview.config.shortcuts.")

        Repeater {
            model: page.shortcutChoices
            RowLayout {
                required property var modelData
                Layout.fillWidth: true
                Text {
                    text: shell.tr(modelData.label)
                    color: Theme.text
                    font.family: Theme.font
                    Layout.fillWidth: true
                }
                SoftSwitch {
                    checked: (page.config.shortcuts || ["files", "terminal", "settings"]).indexOf(modelData.id) >= 0
                    onToggled: page.setShortcut(modelData.id, checked)
                }
            }
        }
    }

    SettingsCard {
        title: shell.tr("Calendar image")
        description: shell.tr("The clock opens the calendar. Drop a local image onto the calendar image area to personalize it.")
        Text {
            Layout.fillWidth: true
            text: page.config.calendarImage || shell.tr("No custom calendar image")
            color: Theme.muted
            font.family: Theme.font
            wrapMode: Text.WrapAnywhere
        }
        ShellButton {
            visible: String(page.config.calendarImage || "").length > 0
            text: shell.tr("Clear calendar image")
            onClicked: page.updateConfig("calendarImage", "")
        }
    }
}
