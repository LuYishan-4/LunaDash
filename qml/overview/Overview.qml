import QtQuick
import QtQuick.Layouts
import Quickshell
import Quickshell.Wayland
import "../style"
PanelWindow {
    id: overview
    required property var shell
    property var stats: shell.state.system || ({})
    anchors { bottom: true; left: true }
    margins { bottom: 34; left: 34 }
    implicitWidth: 560; implicitHeight: 350
    exclusionMode: ExclusionMode.Ignore
    WlrLayershell.layer: WlrLayer.Bottom
    WlrLayershell.namespace: "ludash-overview"
    color: "transparent"
    Rectangle {
        anchors.fill: parent; radius: 8
        color: "#d1121c1d"; border.width: 1; border.color: Theme.accent
        RowLayout {
            anchors.fill: parent; anchors.margins: 25; spacing: 23
            Rectangle {
                Layout.preferredWidth: 133; Layout.fillHeight: true
                color: "#172426"; clip: true; radius: 3
                Image { anchors.fill: parent; source: shell.state.wallpaperImage || ""; fillMode: Image.PreserveAspectCrop; sourceSize: Qt.size(512, 768); asynchronous: true }
                Rectangle { anchors.fill: parent; color: "#142527"; opacity: 0.2 }
                Column {
                    anchors { left: parent.left; right: parent.right; bottom: parent.bottom; margins: 12 }
                    spacing: 4
                    Text { text: "LuDash"; color: "#e4eae6"; font.pixelSize: 24; font.family: Theme.font }
                    Text { text: "WAYLAND / 0.1"; color: Theme.accent; font.family: Theme.font; font.pixelSize: 10 }
                }
            }
            ColumnLayout {
                Layout.fillWidth: true; Layout.fillHeight: true; spacing: 5
                Text { text: (shell.state.appearance || {}).showHostDetails ? (overview.stats.user || "user") + " @ " + (overview.stats.host || "linux") : "LuDash / " + shell.tr("Your workspace"); color: Theme.accent; font.family: Theme.font; font.pixelSize: 13; elide: Text.ElideRight; Layout.fillWidth: true }
                Text { text: "────────────────────────────"; color: Theme.border; font.family: Theme.font; font.pixelSize: 11 }
                Repeater {
                    model: [
                        ["OS", (overview.stats.os || "Linux")],
                        ["KER", overview.stats.kernel || "—"],
                        ["WM", "LuDash (Wayland)"],
                        ["UI", "Quickshell"],
                        ["GL", shell.state.graphicsApi || "—"]
                    ]
                    RowLayout {
                        Layout.fillWidth: true
                        required property var modelData
                        Text { text: modelData[0]; color: Theme.accent; font.family: Theme.font; font.pixelSize: 12; Layout.preferredWidth: 32 }
                        Text { text: modelData[1]; color: Theme.text; font.family: Theme.font; font.pixelSize: 12; elide: Text.ElideRight; Layout.fillWidth: true }
                    }
                }
                Item { Layout.preferredHeight: 6 }
                Text { text: overview.stats.cpuModel || "CPU"; color: Theme.muted; font.family: Theme.font; font.pixelSize: 11; elide: Text.ElideRight; Layout.fillWidth: true }
                Text { text: "CPU  " + (overview.stats.cpuPercent || 0) + "%"; color: Theme.text; font.family: Theme.font; font.pixelSize: 12 }
                Text { text: "MEM  " + Number(overview.stats.memoryUsed || 0).toFixed(1) + " / " + Number(overview.stats.memoryTotal || 0).toFixed(1) + " GiB"; color: Theme.text; font.family: Theme.font; font.pixelSize: 12 }
                Text { text: "DIS  " + Number(overview.stats.diskUsed || 0).toFixed(1) + " / " + Number(overview.stats.diskTotal || 0).toFixed(1) + " GiB"; color: Theme.text; font.family: Theme.font; font.pixelSize: 12 }
                Item { Layout.fillHeight: true }
                Row {
                    Repeater { model: ["#273438", "#a98483", "#91ae96", "#b8b59a", "#7dcccf", "#b4a3bf", "#91afb0", "#d8dfd8"]; Rectangle { required property var modelData; width: 29; height: 20; color: modelData } }
                }
            }
        }
    }
}
