import "../modules"
import QtQuick
import QtQuick.Layouts
import Quickshell
import Quickshell.Wayland
import "../components"
import "../style"
ModuleSurface {
    id: dashboard
    moduleId: "overview"
    property var stats: shell.state.system || ({})
    property int tab: 0
    property string time: ""
    property string date: ""
    anchors { top: true }
    margins.top: Theme.barHeight + moduleMargin
    implicitWidth: moduleWidth(760); implicitHeight: moduleHeight(390)
    exclusionMode: ExclusionMode.Ignore
    WlrLayershell.layer: WlrLayer.Top
    WlrLayershell.namespace: "lunadash-overview"
    color: "transparent"
    Timer { interval: 1000; running: true; repeat: true; triggeredOnStart: true; onTriggered: { dashboard.time = Qt.formatDateTime(new Date(), "HH:mm"); dashboard.date = Qt.formatDateTime(new Date(), "dddd, d MMMM") } }
    Rectangle { anchors.fill: parent; color: moduleBackground; radius: moduleRadius }
    ColumnLayout {
        anchors.fill: parent; anchors.margins: 24; spacing: 18
        RowLayout {
            Repeater {
                model: ["Dashboard", "Performance", "Workspaces"]
                ShellButton { required property string modelData; required property int index; text: shell.tr(modelData); active: dashboard.tab === index; Layout.fillWidth: true; onClicked: dashboard.tab = index }
            }
            ShellButton { text: "×"; onClicked: shell.setAppearance({ overview: false }) }
        }
        Rectangle { Layout.fillWidth: true; height: 1; color: Theme.border }
        RowLayout {
            visible: dashboard.tab === 0; Layout.fillWidth: true; Layout.fillHeight: true; spacing: 22
            Rectangle {
                Layout.preferredWidth: 195; Layout.fillHeight: true; radius: 24; color: Theme.surface; clip: true
                Image { anchors.fill: parent; source: shell.state.wallpaperImage || ""; fillMode: Image.PreserveAspectCrop; sourceSize: Qt.size(390, 520); asynchronous: true }
                Rectangle { anchors.fill: parent; color: "#50101418" }
                Column {
                    anchors { left: parent.left; bottom: parent.bottom; margins: 18 } spacing: 3
                    Text { text: "LunaDash"; color: "white"; font.pixelSize: 28; font.weight: Font.Medium }
                    Text { text: "WAYLAND / 0.1"; color: moduleAccent; font.pixelSize: 10 }
                }
            }
            ColumnLayout {
                Layout.fillWidth: true; Layout.fillHeight: true; spacing: 7
                Text { text: dashboard.time; font.pixelSize: 60; font.weight: Font.Light; color: moduleAccent }
                Text { text: dashboard.date; color: Theme.muted; font.pixelSize: 14 }
                Text { text: (shell.state.appearance || {}).showHostDetails ? (dashboard.stats.user || "user") + " @ " + (dashboard.stats.host || "linux") : shell.tr("Your workspace"); color: moduleForeground; font.pixelSize: 19; Layout.topMargin: 12 }
                Text { text: (dashboard.stats.os || "Linux") + " · " + (shell.state.graphicsApi || "OpenGL"); color: Theme.muted; font.pixelSize: 12 }
                Text { text: shell.tr((shell.state.network || {}).label || "Checking network"); color: Theme.muted; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                Item { Layout.fillHeight: true }
                RowLayout {
                    ShellButton { text: shell.tr("Files"); onClicked: shell.launch("files") }
                    ShellButton { text: shell.tr("Terminal"); onClicked: shell.launch("terminal") }
                    ShellButton { text: shell.tr("Desktop settings"); onClicked: { shell.setAppearance({ overview: false }); shell.settingsOpen = true } }
                }
            }
        }
        ColumnLayout {
            visible: dashboard.tab === 1; Layout.fillHeight: true; Layout.fillWidth: true; spacing: 15
            Text { text: dashboard.stats.cpuModel || "CPU"; color: moduleForeground; font.pixelSize: 17; elide: Text.ElideRight; Layout.fillWidth: true }
            RowLayout {
                Layout.fillWidth: true; Layout.fillHeight: true; spacing: 12
                Repeater {
                    model: [
                        ["CPU", (dashboard.stats.cpuPercent || 0) + "%"],
                        ["RAM", Number(dashboard.stats.memoryUsed || 0).toFixed(1) + " GiB"],
                        ["DISK", Number(dashboard.stats.diskUsed || 0).toFixed(0) + " GiB"]
                    ]
                    Rectangle {
                        required property var modelData
                        Layout.fillWidth: true; Layout.fillHeight: true; radius: 22; color: Theme.surface
                        Column { anchors.centerIn: parent; spacing: 12
                            Text { text: modelData[0]; color: Theme.muted; font.pixelSize: 12 }
                            Text { text: modelData[1]; color: moduleAccent; font.pixelSize: 29; font.weight: Font.Light }
                        }
                    }
                }
            }
            Text { text: "Kernel  " + (dashboard.stats.kernel || "—") + "    ·    " + shell.tr("Live system statistics"); color: Theme.muted; font.pixelSize: 12 }
            ShellButton { text: shell.tr("System monitor"); onClicked: shell.launch("monitor") }
        }
        RowLayout {
            visible: dashboard.tab === 2; Layout.fillWidth: true; Layout.fillHeight: true; spacing: 14
            Repeater {
                model: (shell.state.appearance || {}).workspaceCount || 4
                Rectangle {
                    required property int index
                    property int count: shell.state.clients.filter(client => client.workspace === index && client.mapped).length
                    Layout.fillWidth: true; Layout.fillHeight: true; radius: 24; color: shell.state.workspace === index ? "#344f69" : Theme.surface
                    Column { anchors.centerIn: parent; spacing: 15
                        Text { text: String(index + 1); font.pixelSize: 48; color: moduleAccent }
                        Text { text: count + " " + shell.tr("windows"); color: Theme.muted; font.pixelSize: 12 }
                    }
                    MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: shell.command("workspace", index) }
                }
            }
        }
    }
}
