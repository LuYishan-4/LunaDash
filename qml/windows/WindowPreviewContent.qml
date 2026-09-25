import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components"
import "../style"

Rectangle {
    id: selector
    required property var shell
    required property var selection
    readonly property var windows: selection.windows || []
    color: Theme.surfaceStrong
    radius: Theme.radiusHero
    border.color: Theme.hairline
    clip: true
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 14
        Text {
            Layout.fillWidth: true
            text: selector.shell.tr("Windows in this workspace")
            color: Theme.text
            font.family: Theme.font
            font.pixelSize: 18
            font.bold: true
            elide: Text.ElideRight
        }
        ListView {
            id: cards
            Layout.fillWidth: true
            Layout.fillHeight: true
            orientation: ListView.Horizontal
            spacing: 12
            clip: true
            model: selector.windows
            currentIndex: Number(selector.selection.index || 0)
            highlightMoveDuration: Theme.motionFast
            onCurrentIndexChanged: positionViewAtIndex(currentIndex, ListView.Contain)
            ScrollBar.horizontal: ScrollBar {}
            delegate: Rectangle {
                id: card
                required property var modelData
                required property int index
                readonly property bool selected: index === cards.currentIndex
                width: Math.min(300, Math.max(160, cards.width * 0.42))
                height: Math.max(1, cards.height - 14)
                radius: Theme.radius
                color: selected ? Theme.surfaceElevated : Theme.background
                border.width: selected ? 2 : 1
                border.color: selected ? Theme.accent : Theme.hairline
                Accessible.role: Accessible.Button
                Accessible.name: String(modelData.title || modelData.appId || selector.shell.tr("Application window"))
                Accessible.focused: selected
                Accessible.onPressAction: choose()
                function choose() {
                    selector.shell.command("switch-window", modelData.id)
                    selector.shell.command("switch-accept", "")
                }
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 10
                    Item {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        Image {
                            id: thumbnail
                            anchors.fill: parent
                            source: card.modelData.thumbnail || ""
                            asynchronous: true
                            cache: false
                            fillMode: Image.PreserveAspectFit
                        }
                        ApplicationIcon {
                            anchors.centerIn: parent
                            width: 48; height: 48
                            visible: thumbnail.status !== Image.Ready
                            shell: selector.shell
                            iconName: String(card.modelData.icon || "")
                            appId: String(card.modelData.appId || "")
                            title: String(card.modelData.title || "")
                        }
                    }
                    Text {
                        Layout.fillWidth: true
                        text: String(card.modelData.title || card.modelData.appId || "")
                        elide: Text.ElideRight
                        color: Theme.text
                        font.family: Theme.font
                        font.pixelSize: 13
                    }
                    Text {
                        Layout.fillWidth: true
                        text: card.modelData.minimized ? selector.shell.tr("Minimized") : String(card.modelData.appId || "")
                        elide: Text.ElideRight
                        color: Theme.muted
                        font.family: Theme.font
                        font.pixelSize: 11
                    }
                }
                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: true
                    onEntered: selector.shell.command("switch-window", card.modelData.id)
                    onClicked: card.choose()
                }
            }
        }
        Text {
            Layout.fillWidth: true
            text: selector.shell.tr("Tab / Shift+Tab to select · Release Alt to focus · Esc to cancel")
            color: Theme.muted
            font.family: Theme.font
            font.pixelSize: 11
            elide: Text.ElideRight
        }
    }
}
