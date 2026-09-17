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
    readonly property var appearance: shell.state.appearance || ({updateChannel:"stable"})
    readonly property var update: shell.state.update || ({status:"idle",currentVersion:"0.1.0",channel:"stable"})
    readonly property var install: shell.updateInstall || ({state:"idle",rollback:false,message:"",details:""})
    readonly property var sessionActions: shell.state.sessionActions || ({reboot:false})
    readonly property string selectedChannel: appearance.updateChannel === "dev" ? "dev" : "stable"
    readonly property string latestLabel: selectedChannel === "dev"
        ? String(update.latestCommit || update.currentCommit || "dev").slice(0, 12)
        : String(update.latestVersion || update.currentVersion || "0.1.0")
    readonly property bool installing: install.state === "running"
    readonly property bool installCompleted: install.state === "completed"
    readonly property bool installFailed: install.state === "error"
    spacing: 16

    PageTitle { shell: page.shell; title: "About LunaDash" }

    SettingsComponents.SettingsCard {
        title: ""
        RowLayout {
            Layout.fillWidth: true; spacing: 22
            LunaDashLogo { Layout.preferredWidth: 126; Layout.preferredHeight: 126; animated: Theme.animations }
            ColumnLayout {
                Layout.fillWidth: true; spacing: 6
                Text { text: "LunaDash " + (page.update.currentVersion || "0.1.0"); color: Theme.text; font.family: Theme.font; font.pixelSize: 28; font.bold: true }
                Text { text: shell.tr("A moonlit, focused Linux desktop."); color: Theme.muted; font.family: Theme.font; font.pixelSize: 13 }
                Text { visible: Boolean(page.update.currentCommit); text: (page.selectedChannel === "dev" ? "dev · " : "") + String(page.update.currentCommit || "").slice(0, 12); color: Theme.muted; font.family: Theme.font; font.pixelSize: 10 }
            }
        }
    }

    SettingsComponents.SettingsCard {
        title: shell.tr("Software updates")
        description: page.selectedChannel === "dev" ? shell.tr("Development follows commits on the dev branch.") : shell.tr("Stable follows published GitHub Releases from main.")

        RowLayout {
            Layout.fillWidth: true; spacing: 10
            Text { text: shell.tr("Channel"); color: Theme.muted; font.family: Theme.font; font.pixelSize: 12 }
            StyledComboBox {
                model: [shell.tr("Stable · releases"), shell.tr("Development · dev commits")]
                currentIndex: page.selectedChannel === "dev" ? 1 : 0
                enabled: !page.installing
                onActivated: {
                    const channel = currentIndex === 1 ? "dev" : "stable"
                    shell.command("appearance", JSON.stringify({updateChannel: channel}))
                    shell.command("check-update", "")
                }
            }
            Item { Layout.fillWidth: true }
            ShellButton {
                text: page.update.status === "checking" ? shell.tr("Checking…") : shell.tr("Check now")
                enabled: page.update.status !== "checking" && !page.installing
                onClicked: shell.command("check-update", "")
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 84
            radius: 14
            color: page.installCompleted
                ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.12)
                : page.installFailed
                    ? Qt.rgba(Theme.danger.r, Theme.danger.g, Theme.danger.b, 0.10)
                    : Qt.rgba(Theme.surface.r, Theme.surface.g, Theme.surface.b, 0.66)
            border.width: 1
            border.color: page.installCompleted ? Theme.accent : page.installFailed ? Theme.danger : Theme.border

            RowLayout {
                anchors.fill: parent
                anchors.margins: 14
                spacing: 12

                Rectangle {
                    Layout.preferredWidth: 34
                    Layout.preferredHeight: 34
                    radius: 17
                    color: page.installCompleted ? Theme.accent
                         : page.installFailed ? Theme.danger
                         : page.installing ? Theme.moon
                         : Theme.control
                    Text {
                        anchors.centerIn: parent
                        text: page.installCompleted ? "✓" : page.installFailed ? "!" : page.installing ? "↻" : "☾"
                        color: page.installCompleted || page.installFailed || page.installing ? Theme.accentInk : Theme.text
                        font.family: Theme.font
                        font.pixelSize: 18
                    }
                    RotationAnimation on rotation {
                        running: page.installing && Theme.animations
                        loops: Animation.Infinite
                        from: 0
                        to: 360
                        duration: Math.max(900, Theme.motion * 5)
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 3
                    Text {
                        Layout.fillWidth: true
                        text: page.installing ? shell.tr("Installing update")
                            : page.installCompleted ? shell.tr(page.install.rollback ? "Rollback completed" : "Update completed")
                            : page.installFailed ? shell.tr(page.install.rollback ? "Rollback failed" : "Update failed")
                            : page.update.status === "checking" ? shell.tr("Checking for updates")
                            : page.update.status === "available" ? shell.tr("Update available")
                            : page.update.status === "upToDate" ? shell.tr("Up to date")
                            : shell.tr("Update status")
                        color: Theme.text
                        font.family: Theme.font
                        font.pixelSize: 14
                        font.weight: Font.DemiBold
                    }
                    Text {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        text: page.installing || page.installCompleted || page.installFailed
                            ? shell.tr(page.install.message || "")
                            : page.update.status === "checking" ? shell.tr("Contacting GitHub and checking the selected channel…")
                            : page.update.status === "available" ? shell.tr("A newer LunaDash build is ready to install.")
                            : page.update.status === "upToDate" ? shell.tr("This installation matches the latest selected-channel build.")
                            : page.update.status === "error" ? shell.tr(page.update.error || "The update check failed.")
                            : shell.tr("Choose a channel and check for updates.")
                        color: page.installFailed ? Theme.danger : Theme.muted
                        font.family: Theme.font
                        font.pixelSize: 11
                    }
                }
            }
        }

        Item {
            Layout.fillWidth: true; Layout.preferredHeight: 78
            Rectangle { anchors.left: parent.left; anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter; anchors.leftMargin: 22; anchors.rightMargin: 22; height: 2; radius: 1; color: Theme.border }
            Rectangle { x: 15; anchors.verticalCenter: parent.verticalCenter; width: 16; height: 16; radius: 8; color: Theme.accent }
            Rectangle { anchors.right: parent.right; anchors.rightMargin: 15; anchors.verticalCenter: parent.verticalCenter; width: 16; height: 16; radius: 8; color: page.update.status === "available" || page.installCompleted ? Theme.accent : Theme.muted }
            Text { anchors.left: parent.left; anchors.top: parent.top; text: shell.tr("Installed") + "\n" + (page.update.currentVersion || "0.1.0"); color: Theme.text; font.family: Theme.font; font.pixelSize: 11 }
            Text { anchors.right: parent.right; anchors.top: parent.top; horizontalAlignment: Text.AlignRight; text: (page.selectedChannel === "dev" ? "dev" : shell.tr("Latest")) + "\n" + page.latestLabel; color: Theme.text; font.family: Theme.font; font.pixelSize: 11 }
        }

        RowLayout {
            Layout.fillWidth: true; spacing: 10
            LineIcon { width: 24; height: 24; name: "update"; ink: page.update.status === "available" ? Theme.accent : Theme.muted }
            ColumnLayout {
                Layout.fillWidth: true
                Text {
                    Layout.fillWidth: true; wrapMode: Text.WordWrap; color: Theme.text; font.family: Theme.font; font.pixelSize: 12
                    text: page.update.status === "checking" ? shell.tr("Checking for updates…")
                        : page.update.status === "available" ? shell.tr("An update is ready to install.")
                        : page.update.status === "upToDate" ? shell.tr("LunaDash is up to date.")
                        : page.update.status === "error" ? shell.tr(page.update.error || "The update check failed.")
                        : shell.tr("Choose a channel and check for updates.")
                }
                Text { visible: page.update.channel === "dev" && Boolean(page.update.latestMessage); text: page.update.latestMessage || ""; color: Theme.muted; font.family: Theme.font; font.pixelSize: 10; elide: Text.ElideRight; Layout.fillWidth: true }
                Text { visible: Boolean(page.update.checkedAt); text: shell.tr("Last checked: ") + page.update.checkedAt; color: Theme.muted; font.family: Theme.font; font.pixelSize: 10 }
                Text { visible: page.installCompleted && Boolean(page.install.target); text: shell.tr("Installed target: ") + String(page.install.target).slice(0, 18); color: Theme.muted; font.family: Theme.font; font.pixelSize: 10 }
            }
            ShellButton { visible: page.update.status === "available" && Boolean(page.update.releaseUrl); text: shell.tr("Details"); onClicked: Qt.openUrlExternally(page.update.releaseUrl) }
            ShellButton {
                visible: page.update.status === "available" && !page.installCompleted
                active: true
                enabled: !page.installing
                text: page.installing ? shell.tr("Installing…") : shell.tr("Update")
                onClicked: shell.installUpdate(page.selectedChannel, page.selectedChannel === "dev" ? page.update.latestCommit : page.update.latestVersion)
            }
            ShellButton {
                visible: page.installCompleted
                active: true
                enabled: page.sessionActions.reboot ?? false
                text: shell.tr("Reboot now")
                onClicked: shell.command("session-action", "reboot")
            }
        }

        Text {
            visible: page.installCompleted && !(page.sessionActions.reboot ?? false)
            Layout.fillWidth: true
            text: shell.tr("The update finished, but reboot is unavailable in this session. Reboot from the host system to use the new installation.")
            color: Theme.muted
            font.family: Theme.font
            font.pixelSize: 10
            wrapMode: Text.WordWrap
        }

        Text {
            visible: (page.installFailed || page.installCompleted) && Boolean(page.install.details)
            Layout.fillWidth: true
            text: page.install.details
            color: Theme.muted
            font.family: "monospace"
            font.pixelSize: 9
            wrapMode: Text.WrapAnywhere
            maximumLineCount: 5
            elide: Text.ElideRight
        }

        RowLayout {
            Layout.fillWidth: true
            Item { Layout.fillWidth: true }
            ShellButton { text: shell.tr("Rollback previous update"); enabled: !page.installing; onClicked: shell.rollbackUpdate() }
        }
    }

    SettingsComponents.SettingsCard {
        title: shell.tr("Community and source")
        description: shell.tr("Follow development, report issues, and review the source code.")
        ShellButton { text: "GitHub"; onClicked: Qt.openUrlExternally(page.update.repositoryUrl || "https://github.com/LuYishan-4/LunaDash") }
    }

    SettingsComponents.SettingsCard {
        title: shell.tr("System information")
        GridLayout {
            Layout.fillWidth: true; columns: 2
            Text { text: shell.tr("Operating system"); color: Theme.muted; font.family: Theme.font } Text { Layout.fillWidth: true; text: page.system.os || "Linux"; color: Theme.text; font.family: Theme.font }
            Text { text: shell.tr("Kernel and architecture"); color: Theme.muted; font.family: Theme.font } Text { Layout.fillWidth: true; text: (page.system.kernel || "—") + " · " + (page.system.architecture || "—"); color: Theme.text; font.family: Theme.font }
            Text { text: shell.tr("Graphics API"); color: Theme.muted; font.family: Theme.font } Text { Layout.fillWidth: true; text: (shell.state.graphicsApi || "OpenGL") + " " + (shell.state.graphicsMajor || 0) + "." + (shell.state.graphicsMinor || 0); color: Theme.text; font.family: Theme.font }
        }
    }
}
