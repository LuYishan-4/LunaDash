import QtQuick
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
        {code:"en_US", name:"English"},
        {code:"zh_TW", name:"Traditional Chinese"},
        {code:"zh_CN", name:"Simplified Chinese"},
        {code:"ja_JP", name:"Japanese"}
    ]
    readonly property var appearance: shell.state.appearance || ({})
    spacing: 16

    PageTitle { shell: page.shell; title: "General" }
    SettingsComponents.SettingsCard {
        title: shell.tr("Language and region")
        description: shell.tr("Choose the interface language. More language packs can be added later without changing this layout.")
        GridLayout {
            columns: page.width < 500 ? 1 : 2
            Layout.fillWidth: true
            Text { wrapMode: Text.Wrap; Layout.minimumWidth: 0; text: shell.tr("Interface language"); color: Theme.text; font.family: Theme.font; Layout.fillWidth: true }
            StyledComboBox {
                Layout.fillWidth: true
                id: language
                translationContext: page.shell
                model: page.languages
                textRole: "name"
                valueRole: "code"
                currentIndex: Math.max(0, page.languages.findIndex(item => item.code === shell.state.language))
                onActivated: shell.setLanguage(currentValue)
                Accessible.name: shell.tr("Interface language")
            }
        }
    }
    SettingsComponents.SettingsCard {
        title: shell.tr("Typography and time")
        description: shell.tr("Adjust the shell typeface and clock format.")
        GridLayout {
            columns: page.width < 500 ? 1 : 2
            Layout.fillWidth: true
            Text { wrapMode: Text.Wrap; Layout.minimumWidth: 0; text: shell.tr("Shell font"); color: Theme.text; font.family: Theme.font; Layout.fillWidth: true }
            StyledComboBox { Layout.fillWidth:true; translationContext: page.shell; model:["sans-serif","serif","monospace"]; currentIndex:model.indexOf(page.appearance.fontFamily||"sans-serif"); onActivated:shell.setAppearance({fontFamily:currentText}) }
        }
        ShellButton { iconName:"general"; text:shell.tr("24-hour clock"); active:page.appearance.clock24Hour??true; onClicked:shell.setAppearance({clock24Hour:!(page.appearance.clock24Hour??true)}) }
    }
    SettingsComponents.SettingsCard {
        title: shell.tr("Notifications")
        description: shell.tr("Notifications appear from the bottom-right corner and can expose details for actionable events such as application crashes.")
        RowLayout { Layout.fillWidth:true
            Text { wrapMode: Text.Wrap; Layout.minimumWidth: 0; text:shell.tr("Desktop notifications"); color:Theme.text; font.family:Theme.font; Layout.fillWidth:true }
            SoftSwitch { checked:page.appearance.notificationsEnabled??true; onToggled:shell.setAppearance({notificationsEnabled:checked}) }
        }
        RowLayout { Layout.fillWidth:true
            Text { wrapMode: Text.Wrap; Layout.minimumWidth: 0; text:shell.tr("Application crash alerts"); color:Theme.text; font.family:Theme.font; Layout.fillWidth:true }
            SoftSwitch { checked:page.appearance.crashNotifications??true; enabled:page.appearance.notificationsEnabled??true; onToggled:shell.setAppearance({crashNotifications:checked}) }
        }
        ShellButton { iconName:"info"; text:shell.tr("Send test notification"); enabled:page.appearance.notificationsEnabled??true; onClicked:shell.notify(shell.tr("LunaDash notification"),shell.tr("Notifications are enabled."),"info","") }
    }
    SettingsComponents.SettingsCard {
        title:shell.tr("Setup and recovery")
        description:shell.tr("Preferences are saved automatically. Resetting does not remove personal files or network profiles.")
        RowLayout {
            ShellButton{iconName:"settings";text:shell.tr("First-run guide");onClicked:{shell.settingsOpen=false;shell.command("setup","")}}
            ShellButton{iconName:"warning";destructive:page.confirmReset;text:shell.tr("Reset desktop preferences");onClicked:page.confirmReset=!page.confirmReset}
        }
        RowLayout {
            visible:page.confirmReset
            opacity:visible?1:0
            ShellButton{iconName:"warning";destructive:true;text:shell.tr("Restore defaults");onClicked:{shell.command("reset-preferences","");page.confirmReset=false}}
            ShellButton{text:shell.tr("Cancel");quiet:true;onClicked:page.confirmReset=false}
            Behavior on opacity{NumberAnimation{duration:Theme.motionFast}}
        }
    }
}
