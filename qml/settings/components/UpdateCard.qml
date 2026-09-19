import QtQuick
import QtQuick.Layouts
import "../../components"
import "../../style"

SettingsCard {
    id: card
    required property var shell
    UpdateState { id: updateState; shell: card.shell }
    readonly property bool canReboot: (shell.state.sessionActions || {}).reboot ?? false
    title: shell.tr("Software updates")
    description: updateState.channel === "dev" ? shell.tr("Development follows commits on the dev branch.") : shell.tr("Stable follows published GitHub Releases from main.")
    emphasized: updateState.installing || updateState.restartRequired

    RowLayout {
        Layout.fillWidth: true
        spacing: 12
        StyledComboBox {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            model: [shell.tr("Stable · releases"), shell.tr("Development · dev commits")]
            currentIndex: updateState.channel === "dev" ? 1 : 0
            enabled: !updateState.installing
            onActivated: {
                shell.command("appearance", JSON.stringify({updateChannel: currentIndex === 1 ? "dev" : "stable"}))
                shell.command("check-update", "")
            }
        }
        ShellButton {
            iconName: "update"
            text: shell.tr("Check now")
            busy: updateState.checkStatus === "checking"
            enabled: !updateState.installing
            onClicked: shell.command("check-update", "")
        }
    }

    Text {
        Layout.fillWidth: true
        Layout.minimumWidth: 0
        text: updateState.status === "installing" ? shell.tr(updateState.install.rollback ? "Restoring previous installation" : "Installing update")
            : updateState.status === "restart" ? shell.tr("Restart required")
            : updateState.status === "checking" ? shell.tr("Checking for updates…")
            : updateState.status === "available" ? shell.tr("An update is ready to install.")
            : updateState.status === "upToDate" ? shell.tr("LunaDash is up to date.")
            : updateState.status === "error" ? shell.tr("The update check failed.")
            : shell.tr("Choose a channel and check for updates.")
        color: updateState.status === "error" ? Theme.danger : Theme.text
        font.family: Theme.font
        font.pixelSize: 16
        font.weight: Font.DemiBold
        wrapMode: Text.WordWrap
    }
    Text {
        Layout.fillWidth: true
        visible: updateState.restartRequired || updateState.checkStatus === "error"
        text: updateState.restartRequired ? shell.tr("The installation changed during this session. Restart to use it.") : shell.tr(updateState.update.error || "The update check failed.")
        color: Theme.muted
        font.family: Theme.font
        font.pixelSize: 12
        wrapMode: Text.WordWrap
    }
    GridLayout {
        Layout.fillWidth: true
        columns: width < 360 ? 1 : 2
        columnSpacing: 20
        rowSpacing: 8
        Text { text: shell.tr("Running version"); color: Theme.muted; font.family: Theme.font; wrapMode: Text.WordWrap; Layout.fillWidth: true }
        Text { text: updateState.refLabel(updateState.currentRef); color: Theme.text; font.family: Theme.font; wrapMode: Text.WrapAnywhere; Layout.fillWidth: true }
        Text { text: shell.tr("Latest on this channel"); color: Theme.muted; font.family: Theme.font; wrapMode: Text.WordWrap; Layout.fillWidth: true }
        Text { text: updateState.refLabel(updateState.latestRef); color: Theme.text; font.family: Theme.font; wrapMode: Text.WrapAnywhere; Layout.fillWidth: true }
    }
    ColumnLayout {
        Layout.fillWidth: true
        visible: updateState.installing
        spacing: 8
        RowLayout {
            Layout.fillWidth: true
            Text { text: updateState.stageLabel(updateState.install.stage); color: Theme.text; font.family: Theme.font; wrapMode: Text.WordWrap; Layout.fillWidth: true; Layout.minimumWidth: 0 }
            Text { text: Math.round(updateState.progress) + "%"; color: Theme.accent; font.family: Theme.font }
        }
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 8
            radius: 4
            color: Theme.control
            Rectangle { width: parent.width * updateState.progress / 100; height: parent.height; radius: 4; color: Theme.accent; Behavior on width { NumberAnimation { duration: Theme.motion } } }
        }
        Text { text: shell.tr(updateState.install.message || ""); color: Theme.muted; font.family: Theme.font; wrapMode: Text.WordWrap; Layout.fillWidth: true }
    }
    Text {
        Layout.fillWidth: true
        visible: updateState.update.channel === updateState.channel && Boolean(updateState.update.latestMessage)
        text: updateState.update.latestMessage || ""
        color: Theme.muted
        font.family: Theme.font
        font.pixelSize: 11
        wrapMode: Text.WordWrap
    }
    Text {
        Layout.fillWidth: true
        visible: updateState.update.channel === updateState.channel && Boolean(updateState.update.checkedAt)
        text: shell.tr("Last checked: ") + String(updateState.update.checkedAt || "")
        color: Theme.muted
        font.family: Theme.font
        font.pixelSize: 10
        wrapMode: Text.WrapAnywhere
    }
    Flow {
        Layout.fillWidth: true
        spacing: 10
        ShellButton {
            visible: updateState.canInstall
            active: true
            iconName: "update"
            text: shell.tr("Update")
            onClicked: shell.installUpdate(updateState.channel, updateState.latestRef)
        }
        ShellButton {
            visible: updateState.update.channel === updateState.channel && Boolean(updateState.update.releaseUrl)
            iconName: "github"
            text: shell.tr("Details")
            onClicked: shell.openUrl(updateState.update.releaseUrl)
        }
        ShellButton {
            visible: updateState.restartRequired
            active: true
            enabled: card.canReboot
            iconName: "power"
            text: shell.tr("Reboot now")
            onClicked: shell.command("session-action", "reboot")
        }
    }
    Text {
        Layout.fillWidth: true
        visible: updateState.restartRequired && !card.canReboot
        text: shell.tr("The update finished, but reboot is unavailable in this session. Reboot from the host system to use the new installation.")
        color: Theme.muted
        font.family: Theme.font
        font.pixelSize: 11
        wrapMode: Text.WordWrap
    }
    ColumnLayout {
        Layout.fillWidth: true
        visible: updateState.hasHistory
        spacing: 6
        Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.border }
        Text { text: shell.tr("Last installation"); color: Theme.muted; font.family: Theme.font; font.pixelSize: 11 }
        Text {
            Layout.fillWidth: true
            text: updateState.failed ? shell.tr(updateState.install.rollback ? "Rollback failed." : "Update failed.")
                : shell.tr(updateState.install.rollback ? "Rollback completed" : "Update completed")
            color: updateState.failed ? Theme.danger : Theme.text
            font.family: Theme.font
            wrapMode: Text.WordWrap
        }
        Text {
            Layout.fillWidth: true
            visible: Boolean(updateState.install.target)
            text: String(updateState.install.channel || "") + " · " + String(updateState.install.target || "")
            color: Theme.muted
            font.family: Theme.font
            font.pixelSize: 11
            wrapMode: Text.WrapAnywhere
        }
        Text {
            Layout.fillWidth: true
            visible: updateState.failed && Boolean(updateState.install.message)
            text: shell.tr(updateState.install.message || "")
            color: Theme.muted
            font.family: Theme.font
            font.pixelSize: 11
            wrapMode: Text.WordWrap
        }
        Text {
            Layout.fillWidth: true
            visible: updateState.failed && Boolean(updateState.install.details)
            text: updateState.install.details || ""
            color: Theme.muted
            font.family: Theme.font
            font.pixelSize: 10
            wrapMode: Text.WrapAnywhere
            maximumLineCount: 6
            elide: Text.ElideRight
        }
    }
    ShellButton {
        Layout.maximumWidth: parent.width
        text: shell.tr("Rollback previous update")
        enabled: !updateState.installing
        onClicked: shell.rollbackUpdate()
    }
}
