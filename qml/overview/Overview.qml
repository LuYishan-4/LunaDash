import "../modules"
import QtQuick
import QtQuick.Controls
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
    property real reveal: opened ? 1 : 0

    readonly property var network: shell.state.network || ({})
    readonly property var updateInfo: shell.state.update || ({})
    readonly property string versionText:
        updateInfo.version || updateInfo.currentVersion || "0.1.0"

    anchors.top: true
    margins.top: Theme.barHeight + moduleMargin + 8

    implicitWidth: moduleWidth(1000)
    implicitHeight: moduleHeight(620)

    exclusionMode: ExclusionMode.Ignore

    WlrLayershell.layer: WlrLayer.Top
    WlrLayershell.namespace: "lunadash-overview"

    color: "transparent"

    Rectangle {
        anchors.fill: parent

        radius: Theme.radiusLarge
        color: Theme.surfaceOpaque

        border.width: 1
        border.color: Qt.rgba(
            moduleAccent.r,
            moduleAccent.g,
            moduleAccent.b,
            0.42
        )
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16

        spacing: 10

        RowLayout {
            Layout.fillWidth: true

            Repeater {
                model: [
                    ["Dashboard", "apps"],
                    ["Media", "sound"],
                    ["Performance", "monitor"],
                    ["Weather", "network"],
                    ["Terminal", "terminal"]
                ]

                Rectangle {
                    required property var modelData
                    required property int index

                    Layout.fillWidth: true
                    Layout.preferredHeight: 50

                    radius: 12

                    color: dashboard.tab === index
                        ? Qt.rgba(
                            Theme.secondaryAccent.r,
                            Theme.secondaryAccent.g,
                            Theme.secondaryAccent.b,
                            0.34
                        )
                        : "transparent"

                    Column {
                        anchors.centerIn: parent
                        spacing: 3

                        LineIcon {
                            anchors.horizontalCenter: parent.horizontalCenter

                            width: 18
                            height: 18

                            name: modelData[1]

                            ink: dashboard.tab === index
                                ? Theme.accent
                                : Theme.text
                        }

                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter

                            text: shell.tr(modelData[0])

                            color: dashboard.tab === index
                                ? Theme.accent
                                : Theme.text

                            font.family: Theme.font
                            font.pixelSize: 12
                        }
                    }

                    Rectangle {
                        visible: dashboard.tab === index

                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom

                        height: 2
                        radius: 1

                        color: Theme.accent
                    }

                    MouseArea {
                        anchors.fill: parent

                        cursorShape: Qt.PointingHandCursor

                        onClicked: dashboard.tab = index
                    }
                }
            }

            ShellButton {
                text: "×"

                onClicked: shell.setAppearance({
                    overview: false
                })
            }
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: Theme.border
        }

        RowLayout {
            visible: dashboard.tab === 0

            Layout.fillWidth: true
            Layout.fillHeight: true

            spacing: 12

            Rectangle {
                Layout.preferredWidth: 470
                Layout.fillHeight: true

                radius: 20
                color: Theme.surface

                border.width: 1
                border.color: Theme.border

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 12

                    spacing: 14

                    Rectangle {
                        Layout.preferredWidth: 205
                        Layout.fillHeight: true

                        radius: 16
                        clip: true

                        color: Theme.background

                        Image {
                            anchors.fill: parent

                            source: shell.state.wallpaperImage || ""

                            fillMode: Image.PreserveAspectCrop
                            asynchronous: true

                            sourceSize: Qt.size(500, 700)
                        }

                        Rectangle {
                            anchors.fill: parent
                            color: Qt.rgba(0, 0, 0, 0.18)
                        }

                        Column {
                            anchors.left: parent.left
                            anchors.bottom: parent.bottom
                            anchors.margins: 14

                            Text {
                                text: "LunaDash"

                                color: "white"

                                font.family: Theme.font
                                font.pixelSize: 27
                                font.weight: Font.DemiBold
                            }

                            Text {
                                text: "WAYLAND / " + dashboard.versionText

                                color: Theme.accent

                                font.family: Theme.font
                                font.pixelSize: 10
                            }
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.fillHeight: true

                        spacing: 8

                        Text {
                            text: "LunaDash"

                            color: Theme.text

                            font.family: Theme.font
                            font.pixelSize: 20
                            font.weight: Font.DemiBold
                        }

                        Text {
                            text: shell.tr("A modern Wayland desktop")

                            color: Theme.muted

                            font.family: Theme.font
                            font.pixelSize: 11
                        }

                        Item {
                            Layout.preferredHeight: 4
                        }

                        RowLayout {
                            Layout.fillWidth: true

                            Text {
                                Layout.fillWidth: true

                                text: shell.tr("Version")
                                color: Theme.muted
                            }

                            Text {
                                text: dashboard.versionText
                                color: Theme.text
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true

                            Text {
                                Layout.fillWidth: true

                                text: shell.tr("Session")
                                color: Theme.muted
                            }

                            Text {
                                text: "Wayland"
                                color: Theme.text
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true

                            Text {
                                Layout.fillWidth: true

                                text: shell.tr("Graphics")
                                color: Theme.muted
                            }

                            Text {
                                text: shell.state.graphicsApi || "OpenGL"
                                color: Theme.text
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true

                            Text {
                                Layout.fillWidth: true

                                text: shell.tr("Kernel")
                                color: Theme.muted
                            }

                            Text {
                                text: dashboard.stats.kernel || "—"
                                color: Theme.text
                            }
                        }

                        Item {
                            Layout.fillHeight: true
                        }

                        ShellButton {
                            Layout.fillWidth: true

                            text: shell.tr("Desktop settings")

                            onClicked: {
                                shell.setAppearance({
                                    overview: false
                                })
                                shell.settingsOpen = true
                            }
                        }
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true

                spacing: 12

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 150

                    radius: 20
                    color: Theme.surface

                    border.width: 1
                    border.color: Theme.border

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 18

                        ColumnLayout {
                            Layout.fillWidth: true

                            Text {
                                text: dashboard.time

                                color: Theme.accent

                                font.family: Theme.font
                                font.pixelSize: 44
                                font.weight: Font.Light
                            }

                            Text {
                                text: dashboard.date

                                color: Theme.muted
                                font.family: Theme.font
                            }
                        }

                        ColumnLayout {
                            Text {
                                text: shell.tr("Welcome back,")

                                color: Theme.muted
                                font.family: Theme.font
                            }

                            Text {
                                text:
                                    dashboard.stats.displayName
                                    || dashboard.stats.user
                                    || shell.tr("Welcome")

                                color: Theme.accent

                                font.family: Theme.font
                                font.pixelSize: 18
                                font.weight: Font.DemiBold
                            }
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    radius: 20
                    color: Theme.surface

                    border.width: 1
                    border.color: Theme.border

                    GridLayout {
                        anchors.fill: parent
                        anchors.margins: 18

                        columns: 2

                        columnSpacing: 18
                        rowSpacing: 12

                        Text {
                            text: "CPU"
                            color: Theme.muted
                            font.family: Theme.font
                        }

                        Text {
                            text:
                                Math.round(
                                    dashboard.stats.cpuPercent || 0
                                ) + "%"

                            color: Theme.accent

                            font.family: Theme.font
                            font.pixelSize: 24
                        }

                        Text {
                            text: shell.tr("Memory")
                            color: Theme.muted
                            font.family: Theme.font
                        }

                        Text {
                            text:
                                Math.round(
                                    dashboard.stats.memoryPercent || 0
                                ) + "%"

                            color: Theme.accent

                            font.family: Theme.font
                            font.pixelSize: 24
                        }

                        Text {
                            text: shell.tr("Network")
                            color: Theme.muted
                            font.family: Theme.font
                        }

                        Text {
                            text: dashboard.network.connected
                                ? shell.tr("Connected")
                                : shell.tr("Disconnected")

                            color: dashboard.network.internet
                                ? Theme.accent
                                : Theme.text

                            font.family: Theme.font
                        }

                        Text {
                            text: shell.tr("User")
                            color: Theme.muted
                            font.family: Theme.font
                        }

                        Text {
                            text: dashboard.stats.user || "—"
                            color: Theme.text
                            font.family: Theme.font
                        }
                    }
                }
            }
        }

        MediaCard {
            visible: dashboard.tab === 1

            Layout.fillWidth: true
            Layout.fillHeight: true

            shell: dashboard.shell
        }

        GridLayout {
            visible: dashboard.tab === 2

            Layout.fillWidth: true
            Layout.fillHeight: true

            columns: 2

            columnSpacing: 12
            rowSpacing: 12

            Repeater {
                model: [
                    [
                        "CPU",
                        Math.round(
                            dashboard.stats.cpuPercent || 0
                        ) + "%",
                        dashboard.stats.cpuModel || "CPU"
                    ],
                    [
                        "GPU",
                        Math.round(
                            dashboard.stats.gpuPercent || 0
                        ) + "%",
                        dashboard.stats.gpuModel || "GPU"
                    ],
                    [
                        shell.tr("Memory"),
                        Math.round(
                            dashboard.stats.memoryPercent || 0
                        ) + "%",
                        Number(
                            dashboard.stats.memoryUsed || 0
                        ).toFixed(1) + " GiB"
                    ],
                    [
                        shell.tr("Storage"),
                        Number(
                            dashboard.stats.diskUsed || 0
                        ).toFixed(1) + " GiB",
                        dashboard.stats.diskDevice || "Disk"
                    ]
                ]

                Rectangle {
                    required property var modelData

                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    radius: 20
                    color: Theme.surface

                    border.width: 1
                    border.color: Theme.border

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 18

                        Text {
                            text: modelData[0]

                            color: Theme.text

                            font.family: Theme.font
                            font.pixelSize: 18
                            font.weight: Font.DemiBold
                        }

                        Text {
                            Layout.fillWidth: true

                            text: modelData[2]

                            color: Theme.muted
                            font.family: Theme.font

                            elide: Text.ElideRight
                        }

                        Item {
                            Layout.fillHeight: true
                        }

                        Text {
                            text: modelData[1]

                            color: Theme.accent

                            font.family: Theme.font
                            font.pixelSize: 38
                            font.weight: Font.Light
                        }
                    }
                }
            }
        }

        Rectangle {
            visible: dashboard.tab === 3

            Layout.fillWidth: true
            Layout.fillHeight: true

            radius: 20
            color: Theme.surface

            border.width: 1
            border.color: Theme.border

            ColumnLayout {
                anchors.centerIn: parent

                spacing: 12

                LineIcon {
                    Layout.alignment: Qt.AlignHCenter

                    width: 52
                    height: 52

                    name: "network"
                    ink: Theme.accent
                }

                Text {
                    Layout.alignment: Qt.AlignHCenter

                    text: shell.tr("Weather")

                    color: Theme.text

                    font.family: Theme.font
                    font.pixelSize: 26
                    font.weight: Font.DemiBold
                }

                Text {
                    Layout.alignment: Qt.AlignHCenter

                    text: shell.tr(
                        "Weather information can be supplied by a shell module."
                    )

                    color: Theme.muted
                    font.family: Theme.font
                }
            }
        }

        Rectangle {
            visible: dashboard.tab === 4

            Layout.fillWidth: true
            Layout.fillHeight: true

            radius: 20
            color: Theme.surface

            border.width: 1
            border.color: Theme.border

            ColumnLayout {
                anchors.centerIn: parent

                spacing: 16

                LineIcon {
                    Layout.alignment: Qt.AlignHCenter

                    width: 58
                    height: 58

                    name: "terminal"
                    ink: Theme.accent
                }

                Text {
                    Layout.alignment: Qt.AlignHCenter

                    text: shell.tr("Terminal")

                    color: Theme.text

                    font.family: Theme.font
                    font.pixelSize: 26
                    font.weight: Font.DemiBold
                }

                ShellButton {
                    Layout.alignment: Qt.AlignHCenter

                    text: shell.tr("Open terminal")
                    active: true

                    onClicked: shell.launch("terminal")
                }
            }
        }
    }

    Timer {
        interval: 1000
        repeat: true
        running: true
        triggeredOnStart: true

        onTriggered: {
            const now = new Date()

            dashboard.time = Qt.formatDateTime(
                now,
                Theme.clock24Hour ? "HH:mm" : "h:mm AP"
            )

            dashboard.date = Qt.formatDateTime(
                now,
                "yyyy/MM/dd dddd"
            )
        }
    }
}
