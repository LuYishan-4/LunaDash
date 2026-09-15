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
    readonly property var system: shell.state.system || ({})
    readonly property var update: shell.state.update || ({status:"idle", currentVersion:"0.1.0"})
    spacing: 16

    PageTitle { shell: page.shell; title: "About LunaDash" }

    SettingsComponents.SettingsCard {
        title: ""
        RowLayout {
            Layout.fillWidth: true
            spacing: 22
            LunaDashLogo {
                Layout.preferredWidth: 126
                Layout.preferredHeight: 126
                animated: Theme.animations
            }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 6
                Text { text: "LunaDash " + (page.update.currentVersion || "0.1.0"); color: Theme.text; font.family: Theme.font; font.pixelSize: 28; font.bold: true }
                Text { text: shell.tr("A moonlit, focused Linux desktop."); color: Theme.muted; font.family: Theme.font; font.pixelSize: 13 }
                Rectangle {
                    implicitWidth: previewLabel.implicitWidth + 18
                    implicitHeight: 26
                    radius: 13
                    color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.16)
                    Text { id: previewLabel; anchors.centerIn: parent; text: shell.tr("Development preview"); color: Theme.accent; font.pixelSize: 11; font.bold: true }
                }
            }
        }
    }

    SettingsComponents.SettingsCard {
        title: shell.tr("Community and source")
        description: shell.tr("Follow development, report issues, and review the source code.")
        RowLayout {
            Layout.fillWidth: true
            spacing: 12
            Rectangle {
                width: 46; height: 46; radius: 14
                color: githubMouse.containsMouse ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.22) : Theme.surface
                LineIcon { anchors.centerIn: parent; width: 25; height: 25; name: "github"; ink: Theme.text }
                MouseArea { id: githubMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: Qt.openUrlExternally(page.update.repositoryUrl || "https://github.com/LuYishan-4/LunaDash") }
                ToolTip.visible: githubMouse.containsMouse; ToolTip.text: "GitHub"
            }
            Text {
                Layout.fillWidth: true
                text: shell.tr("Open the LunaDash repository on GitHub to follow development, report issues and review the source code.")
                color: Theme.muted; wrapMode: Text.WordWrap; font.pixelSize: 11
            }
        }
    }

    SettingsComponents.SettingsCard {
        title: shell.tr("Software updates")
        description: shell.tr("Check the official GitHub release feed. This sends a request to GitHub and never installs packages automatically.")
        RowLayout {
            Layout.fillWidth: true
            LineIcon { width: 24; height: 24; name: "update"; ink: page.update.status === "available" ? Theme.accent : Theme.muted }
            ColumnLayout {
                Layout.fillWidth: true
                Text {
                    color: Theme.text
                    text: page.update.status === "checking" ? shell.tr("Checking for updates…")
                        : page.update.status === "available" ? shell.tr("A new upstream release is available: ") + page.update.latestVersion
                        : page.update.status === "upToDate" ? shell.tr("LunaDash is up to date with the latest upstream release.")
                        : page.update.status === "error" ? shell.tr(page.update.error || "The update check failed.")
                        : shell.tr("No update check has been run yet.")
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
                Text { visible: Boolean(page.update.checkedAt); text: shell.tr("Last checked: ") + page.update.checkedAt; color: Theme.muted; font.pixelSize: 10 }
            }
            ShellButton { text: shell.tr("Check now"); enabled: page.update.status !== "checking"; onClicked: shell.command("check-update", "") }
            ShellButton { visible: page.update.status === "available"; text: shell.tr("View release"); onClicked: Qt.openUrlExternally(page.update.releaseUrl) }
        }
    }

    SettingsComponents.SettingsCard {
        title: shell.tr("System information")
        GridLayout {
            Layout.fillWidth: true
            columns: 2
            columnSpacing: 24
            rowSpacing: 9
            Text { text: shell.tr("Operating system"); color: Theme.muted }
            Text { Layout.fillWidth: true; text: page.system.os || "Linux"; color: Theme.text; elide: Text.ElideRight }
            Text { text: shell.tr("Kernel and architecture"); color: Theme.muted }
            Text { Layout.fillWidth: true; text: (page.system.kernel || "—") + " · " + (page.system.architecture || "—"); color: Theme.text; elide: Text.ElideRight }
            Text { text: shell.tr("Graphics API"); color: Theme.muted }
            Text { Layout.fillWidth: true; text: (shell.state.graphicsApi || "OpenGL") + " " + (shell.state.graphicsMajor || 0) + "." + (shell.state.graphicsMinor || 0); color: Theme.text }
            Text { text: shell.tr("Session"); color: Theme.muted }
            Text { Layout.fillWidth: true; text: shell.tr("Wayland compositor with a Quickshell interface"); color: Theme.text; wrapMode: Text.WordWrap }
        }
    }

    SettingsComponents.SettingsCard {
        title: shell.tr("Current limitations")
        description: shell.tr("This development preview does not yet provide secure screen locking, session restoration, complete portals, standalone multi-monitor control, HDR, color management, or a complete input-method popup protocol.")
    }
}
