import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components"
import "../style"
ColumnLayout {
    id: controls
    required property var shell
    spacing: 10
    Text { text: shell.tr("Accent color"); color: Theme.muted; font.family: Theme.font }
    RowLayout {
        Repeater {
            model: ["#7dcccf", "#c4b5fd", "#e7b899", "#a8c89b"]
            ShellButton {
                required property string modelData
                text: modelData; active: Theme.accent.toString() === modelData
                onClicked: shell.setAppearance({ accent: modelData })
                Rectangle { anchors.bottom: parent.bottom; anchors.horizontalCenter: parent.horizontalCenter; width: parent.width - 12; height: 3; color: modelData }
            }
        }
    }
    RowLayout {
        Text { text: shell.tr("Window gaps") + "  " + ((shell.state.appearance || {}).gap ?? 12) + " px"; color: Theme.text; Layout.fillWidth: true }
        ShellButton { text: "−"; enabled: ((shell.state.appearance || {}).gap ?? 12) > 4; onClicked: shell.setAppearance({ gap: Math.max(4, ((shell.state.appearance || {}).gap ?? 12) - 4) }) }
        ShellButton { text: "+"; enabled: ((shell.state.appearance || {}).gap ?? 12) < 32; onClicked: shell.setAppearance({ gap: Math.min(32, ((shell.state.appearance || {}).gap ?? 12) + 4) }) }
    }
    RowLayout {
        Text { text: shell.tr("Panel height") + "  " + Theme.barHeight + " px"; color: Theme.text; Layout.fillWidth: true }
        ShellButton { text: "−"; enabled: Theme.barHeight > 24; onClicked: shell.setAppearance({ panelHeight: Math.max(24, Theme.barHeight - 4) }) }
        ShellButton { text: "+"; enabled: Theme.barHeight < 40; onClicked: shell.setAppearance({ panelHeight: Math.min(40, Theme.barHeight + 4) }) }
    }
    RowLayout {
        ShellButton { text: shell.tr("Desktop information"); active: shell.overviewOpen; onClicked: shell.setAppearance({ overview: !shell.overviewOpen }) }
        ShellButton { text: shell.tr("Show user and host"); active: (shell.state.appearance || {}).showHostDetails ?? false; onClicked: shell.setAppearance({ showHostDetails: !((shell.state.appearance || {}).showHostDetails ?? false) }) }
    }
}
