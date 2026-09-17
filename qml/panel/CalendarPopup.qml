import "../modules"
import QtQuick
import QtQuick.Layouts
import Quickshell
import Quickshell.Wayland
import "../components"
import "../style"

ModuleSurface {
    id: calendar
    moduleId: "overview"
    anchors.top: true
    anchors.right: true
    margins.top: Theme.barHeight + 8
    margins.right: 10
    implicitWidth: moduleWidth(430)
    implicitHeight: moduleHeight(520)
    exclusionMode: ExclusionMode.Ignore
    WlrLayershell.layer: WlrLayer.Overlay
    WlrLayershell.namespace: "lunadash-calendar"
    color: "transparent"

    property date shownMonth: new Date(new Date().getFullYear(), new Date().getMonth(), 1)
    readonly property var overviewModule: (((shell.state.shellModules || {}).modules || {}).overview || ({}))
    readonly property var overviewConfig: overviewModule.config || ({})
    readonly property string calendarImage: overviewConfig.calendarImage || ""
    readonly property var weekdayNames: ["Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"]

    function daysInMonth(date) {
        return new Date(date.getFullYear(), date.getMonth() + 1, 0).getDate()
    }
    function setCalendarImage(url) {
        const document = JSON.parse(JSON.stringify((shell.state.shellModules || {}).document || {schemaVersion:1, modules:{}}))
        if (!document.modules || !document.modules.overview)
            return
        if (!document.modules.overview.config)
            document.modules.overview.config = {}
        document.modules.overview.config.calendarImage = String(url || "")
        shell.command("module-save", JSON.stringify(document))
    }
    function chooseCalendarImage() {
        shell.pickerPurpose = "calendar"
        shell.settingsOpen = true
        shell.pickerOpen = true
    }

    Rectangle {
        anchors.fill: parent
        color: moduleBackground
        radius: moduleRadius
        border.width: 1
        border.color: Qt.rgba(moduleAccent.r, moduleAccent.g, moduleAccent.b, 0.34)
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 18
        spacing: 12

        RowLayout {
            Layout.fillWidth: true
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 1
                Text { text: Qt.formatDate(new Date(), "dddd"); color: moduleAccent; font.family: Theme.font; font.pixelSize: 12 }
                Text { text: Qt.formatDate(new Date(), "d MMMM yyyy"); color: moduleForeground; font.family: Theme.font; font.pixelSize: 20; font.weight: Font.DemiBold }
            }
            ShellButton { text: "×"; Accessible.name: shell.tr("Close calendar"); onClicked: shell.calendarOpen = false }
        }

        Rectangle {
            id: artBox
            Layout.fillWidth: true
            Layout.preferredHeight: 128
            radius: 16
            color: Qt.rgba(moduleAccent.r, moduleAccent.g, moduleAccent.b, 0.09)
            border.width: 1
            border.color: Theme.border
            clip: true

            Image {
                anchors.fill: parent
                source: calendar.calendarImage
                fillMode: Image.PreserveAspectCrop
                asynchronous: true
                visible: source.toString().length > 0 && status !== Image.Error
            }

            Rectangle {
                anchors.fill: parent
                color: "transparent"
                visible: calendar.calendarImage.length === 0
                Column {
                    anchors.centerIn: parent
                    spacing: 6
                    LunaDashLogo { anchors.horizontalCenter: parent.horizontalCenter; width: 42; height: 42; animated: false }
                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: shell.tr("Choose an image for your calendar")
                        color: Theme.muted
                        font.family: Theme.font
                        font.pixelSize: 11
                    }
                }
            }

            Row {
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: 8
                spacing: 6
                ShellButton {
                    text: shell.tr("Change image")
                    active: true
                    onClicked: calendar.chooseCalendarImage()
                }
                ShellButton {
                    visible: calendar.calendarImage.length > 0
                    text: shell.tr("Clear image")
                    onClicked: calendar.setCalendarImage("")
                }
            }

            MouseArea {
                anchors.fill: parent
                anchors.topMargin: 44
                cursorShape: Qt.PointingHandCursor
                onClicked: calendar.chooseCalendarImage()
            }
        }

        RowLayout {
            Layout.fillWidth: true
            ShellButton { text: "‹"; Accessible.name: shell.tr("Previous month"); onClicked: calendar.shownMonth = new Date(calendar.shownMonth.getFullYear(), calendar.shownMonth.getMonth() - 1, 1) }
            Text {
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                text: Qt.formatDate(calendar.shownMonth, "MMMM yyyy")
                color: moduleForeground
                font.family: Theme.font
                font.pixelSize: 15
                font.weight: Font.DemiBold
            }
            ShellButton { text: "›"; Accessible.name: shell.tr("Next month"); onClicked: calendar.shownMonth = new Date(calendar.shownMonth.getFullYear(), calendar.shownMonth.getMonth() + 1, 1) }
        }

        GridLayout {
            Layout.fillWidth: true
            columns: 7
            columnSpacing: 4
            rowSpacing: 4
            Repeater {
                model: calendar.weekdayNames
                Text {
                    required property string modelData
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    text: shell.tr(modelData)
                    color: Theme.muted
                    font.family: Theme.font
                    font.pixelSize: 10
                }
            }
            Repeater {
                model: 42
                Rectangle {
                    required property int index
                    readonly property int day: index - calendar.shownMonth.getDay() + 1
                    readonly property bool validDay: day >= 1 && day <= calendar.daysInMonth(calendar.shownMonth)
                    readonly property date today: new Date()
                    readonly property bool isToday: validDay && day === today.getDate() && calendar.shownMonth.getMonth() === today.getMonth() && calendar.shownMonth.getFullYear() === today.getFullYear()
                    Layout.fillWidth: true
                    Layout.preferredHeight: 34
                    radius: 10
                    color: isToday ? moduleAccent : "transparent"
                    Text {
                        anchors.centerIn: parent
                        text: parent.validDay ? String(parent.day) : ""
                        color: parent.isToday ? moduleBackground : moduleForeground
                        font.family: Theme.font
                        font.pixelSize: 11
                    }
                }
            }
        }

        Item { Layout.fillHeight: true }
        Text {
            Layout.fillWidth: true
            text: shell.tr("Click the image area to choose a local PNG, JPEG, WebP, or GIF with the LunaDash file picker.")
            color: Theme.muted
            font.family: Theme.font
            font.pixelSize: 10
            wrapMode: Text.WordWrap
        }
    }
}
