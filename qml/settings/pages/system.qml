import QtQuick
import QtQuick.Layouts
import "../components"
import "../../components"
import "../../style"

ColumnLayout {
    id: page
    required property var shell
    readonly property var systemInfo: shell.state.system || ({})
    spacing: 16

    PageTitle { shell: page.shell; title: "Users, date and time" }

    SettingsCard {
        title: shell.tr("Current user")
        description: shell.tr("LunaDash reads your display name and local account image from the current session.")
        RowLayout {
            Layout.fillWidth: true
            spacing: 14
            Rectangle {
                width: 70
                height: 70
                radius: 35
                color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.16)
                border.width: 2
                border.color: Theme.accent
                clip: true
                Image {
                    anchors.fill: parent
                    source: page.systemInfo.avatar || ""
                    fillMode: Image.PreserveAspectCrop
                    asynchronous: true
                    visible: source.toString().length > 0 && status !== Image.Error
                }
                LunaDashLogo {
                    anchors.centerIn: parent
                    width: 42
                    height: 42
                    visible: !page.systemInfo.avatar
                    animated: false
                    primaryColor: Theme.accent
                    secondaryColor: Theme.secondaryAccent
                }
            }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 3
                Text {
                    Layout.fillWidth: true
                    text: page.systemInfo.displayName || page.systemInfo.user || shell.tr("User")
                    color: Theme.text
                    font.family: Theme.font
                    font.pixelSize: 18
                    font.weight: Font.DemiBold
                    elide: Text.ElideRight
                }
                Text {
                    Layout.fillWidth: true
                    text: [page.systemInfo.user, page.systemInfo.host].filter(Boolean).join("  ·  ")
                    color: Theme.muted
                    font.family: Theme.font
                    font.pixelSize: 11
                    elide: Text.ElideRight
                }
                Text {
                    Layout.fillWidth: true
                    text: page.systemInfo.os || ""
                    color: Theme.muted
                    font.family: Theme.font
                    font.pixelSize: 11
                    elide: Text.ElideRight
                }
            }
            ShellButton {
                text: shell.tr("Manage users")
                active: true
                onClicked: shell.command("system-tool", "users")
            }
        }
    }

    HelpText { shell: page.shell; message: "Account and clock changes may require system authorization. LunaDash opens the installed system tool without storing passwords." }
    ToolList { shell: page.shell; category: "system" }
}
