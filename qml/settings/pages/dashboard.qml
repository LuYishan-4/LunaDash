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
        description: shell.tr("Open the control center from the panel or Super+I. Turn off compact mode for the full dashboard.")

        RowLayout {
            Layout.fillWidth: true
            Text { Layout.fillWidth: true; text: shell.tr("Compact control center"); color: Theme.text; font.family: Theme.font; wrapMode: Text.Wrap }
            SoftSwitch { checked: page.config.compactControlCenter ?? true; onToggled: page.updateConfig("compactControlCenter", checked) }
        }
        RowLayout {
            Layout.fillWidth: true
            Text { wrapMode: Text.Wrap; Layout.minimumWidth: 0; text: shell.tr("Media player"); color: Theme.text; font.family: Theme.font; Layout.fillWidth: true }
            SoftSwitch {
                checked: page.config.showMedia ?? true
                onToggled: page.updateConfig("showMedia", checked)
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Text { wrapMode: Text.Wrap; Layout.minimumWidth: 0; text: shell.tr("Volume and Wi-Fi quick controls"); color: Theme.text; font.family: Theme.font; Layout.fillWidth: true }
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
                Text { wrapMode: Text.Wrap; Layout.minimumWidth: 0;
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
        title: shell.tr("Calendar artwork")
        description: shell.tr("Click the artwork itself to replace it. There are no separate choose or clear buttons.")
        interactive: true
        onActivated: {
            shell.pickerPurpose = "calendar"
            shell.pickerOpen = true
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 132
            radius: Theme.radiusMedium
            color: Theme.control
            border.width: 1
            border.color: Theme.hairline
            clip: true

            Image {
                anchors.fill: parent
                source: page.config.calendarImage || ""
                fillMode: Image.PreserveAspectCrop
                asynchronous: true
                visible: source.toString().length > 0 && status !== Image.Error
            }
            Column {
                anchors.centerIn: parent
                spacing: 6
                visible: !page.config.calendarImage
                LineIcon { anchors.horizontalCenter: parent.horizontalCenter; width: 30; height: 30; name: "appearance"; ink: Theme.accent }
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: shell.tr("Click to choose calendar artwork")
                    color: Theme.muted
                    font.family: Theme.font
                    font.pixelSize: 11
                }
            }
            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    shell.pickerPurpose = "calendar"
                    shell.pickerOpen = true
                }
            }
        }
    }
}
