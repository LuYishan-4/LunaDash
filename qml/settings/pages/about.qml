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
    readonly property bool compact: width < 600
    spacing: 16

    PageTitle { shell: page.shell; title: "About LunaDash" }

    SettingsComponents.SettingsCard {
        emphasized: true
        title: ""
        GridLayout {
            Layout.fillWidth: true
            columns: page.compact ? 1 : 2
            columnSpacing: 24
            rowSpacing: 12
            LunaDashLogo {
                Layout.preferredWidth: page.compact ? 88 : 126
                Layout.preferredHeight: page.compact ? 88 : 126
                Layout.alignment: Qt.AlignVCenter
                animated: Theme.animations
            }
            ColumnLayout {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                spacing: 8
                Text {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    text: "LunaDash"
                    color: Theme.text
                    font.family: Theme.font
                    font.pixelSize: 32
                    font.weight: Font.DemiBold
                    elide: Text.ElideRight
                }
                Text {
                    Layout.fillWidth: true
                    text: shell.tr("Your desktop. Your rhythm.")
                    color: Theme.accent
                    font.family: Theme.font
                    font.pixelSize: 15
                    wrapMode: Text.Wrap
                }
                Text {
                    Layout.fillWidth: true
                    text: shell.tr("A modular Wayland desktop with a live, personal workspace.")
                    color: Theme.muted
                    font.family: Theme.font
                    wrapMode: Text.Wrap
                }
                Flow {
                    Layout.fillWidth: true
                    spacing: 6
                    Repeater {
                        model: [page.update.currentVersion || "1.0.1a", "Wayland", "wlroots", "Qt Quick"]
                        Rectangle {
                            required property string modelData
                            width: label.implicitWidth + 20
                            height: 27
                            radius: 13
                            color: Theme.surfaceGlass
                            border.color: Theme.hairline
                            Text {
                                id: label
                                anchors.centerIn: parent
                                text: modelData
                                color: Theme.text
                                font.family: Theme.font
                                font.pixelSize: 10
                            }
                        }
                    }
                }
                Text {
                    Layout.fillWidth: true
                    visible: Boolean(page.update.currentCommit)
                    text: shell.tr("Revision") + "  " + String(page.update.currentCommit || "").slice(0, 12)
                    color: Theme.muted
                    font.family: Theme.font
                    font.pixelSize: 10
                    elide: Text.ElideRight
                }
            }
        }
    }

    GridLayout {
        Layout.fillWidth: true
        columns: page.compact ? 1 : 3
        columnSpacing: 12
        rowSpacing: 12
        Repeater {
            model: [
                {name:"CPU", value: Math.round(page.system.cpuPercent || 0) + "%", detail: page.system.cpuModel || "—"},
                {name:"Memory", value: Number(page.system.memoryTotal || 0).toFixed(1) + " GiB",
                    detail: Number(page.system.memoryUsed || 0).toFixed(1) + " GiB " + shell.tr("in use")},
                {name:"GPU", value: page.system.gpuAvailable ? Math.round(page.system.gpuPercent) + "%" : "—",
                    detail: page.system.gpuModel || page.system.gpuDriver || shell.tr("GPU counters unavailable")}
            ]
            SettingsComponents.SettingsCard {
                required property var modelData
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                Layout.fillHeight: true
                title: shell.tr(modelData.name)
                Text {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    text: modelData.value
                    color: Theme.accent
                    font.family: Theme.font
                    font.pixelSize: 25
                    elide: Text.ElideRight
                }
                Text {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    text: modelData.detail
                    textFormat: Text.PlainText
                    color: Theme.muted
                    font.family: Theme.font
                    font.pixelSize: 11
                    wrapMode: Text.Wrap
                    maximumLineCount: 3
                    elide: Text.ElideRight
                }
            }
        }
    }

    SettingsComponents.UpdateCard { shell: page.shell }

    SettingsComponents.SettingsCard {
        title: shell.tr("System information")
        GridLayout {
            Layout.fillWidth: true
            columns: 2
            columnSpacing: 16
            rowSpacing: 12
            Repeater {
                model: [shell.tr("Operating system"), page.system.os || "Linux",
                    shell.tr("Kernel and architecture"), (page.system.kernel || "—") + " · " + (page.system.architecture || "—"),
                    shell.tr("Graphics API"), (shell.state.graphicsApi || "OpenGL") + " " + (shell.state.graphicsMajor || 0) + "." + (shell.state.graphicsMinor || 0),
                    shell.tr("Display server"), "Wayland · " + ((shell.state.xwayland || {}).running ? "XWayland" : "wlroots"),
                    shell.tr("Visual effects"), shell.tr((shell.state.appearance || {}).animations === false ? "Reduced motion" : "Animations enabled")]
                Text {
                    required property string modelData
                    required property int index
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    text: modelData
                    textFormat: Text.PlainText
                    color: index % 2 ? Theme.text : Theme.muted
                    font.family: Theme.font
                    wrapMode: Text.Wrap
                }
            }
        }
    }

    SettingsComponents.SettingsCard {
        title: shell.tr("Community and source")
        description: shell.tr("Follow development, report issues, and review the source code.")
        GridLayout {
            Layout.fillWidth: true
            columns: page.compact ? 1 : 3
            ShellButton {
                Layout.fillWidth: true
                iconName: "github"
                text: shell.tr("Source code")
                onClicked: shell.openUrl("https://github.com/LuYishan-4/LunaDash")
            }
            ShellButton {
                Layout.fillWidth: true
                text: shell.tr("Documentation")
                onClicked: shell.openUrl("https://luyishan-4.github.io/LunaDash/")
            }
            ShellButton {
                Layout.fillWidth: true
                text: shell.tr("Report an issue")
                onClicked: shell.openUrl("https://github.com/LuYishan-4/LunaDash/issues")
            }
        }
        Text {
            Layout.fillWidth: true
            text: shell.tr("Built by the LunaDash community, with thanks to the free software projects underneath.")
            color: Theme.muted
            font.family: Theme.font
            font.pixelSize: 11
            wrapMode: Text.Wrap
        }
    }
}
