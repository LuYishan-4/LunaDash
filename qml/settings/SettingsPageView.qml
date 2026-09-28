import QtQuick
import QtQuick.Controls
import "../plugins"
import "../style"

ScrollView {
    id: view
    required property var shell
    property string category: "general"
    property url pageSource: Qt.resolvedUrl("pages/" + category + ".qml")
    readonly property alias pageItem: pageLoader.item
    readonly property var firstFocusItem:
        pageLoader.item && pageLoader.item.firstFocusItem
            ? pageLoader.item.firstFocusItem : null
    readonly property bool ready: pageLoader.status === Loader.Ready
    property bool initialized: false
    clip: true
    contentWidth: availableWidth
    // The extension wrapper is an Item, whose implicit height otherwise stays
    // zero even when the loaded page is taller than the visible settings card.
    contentHeight: pageExtension.implicitHeight
    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
    ScrollBar.vertical.policy: ScrollBar.AsNeeded
    ScrollBar.vertical.interactive: true

    function resetPosition() {
        contentItem.cancelFlick();
        contentItem.contentY = 0;
    }
    function loadPage() {
        if (initialized)
            pageLoader.setSource(pageSource, {
                shell: view.shell
            });
    }
    onPageSourceChanged: loadPage()
    Component.onCompleted: {
        initialized = true;
        loadPage();
    }

    ExtensionSlot {
        id: pageExtension
        shell: view.shell
        target: "settings." + view.category
        forceBuiltin: view.category === "modules" || view.category === "plugins"
        context: ({
                page: view.category
            })
        width: Math.max(0, view.availableWidth - 12)
        // Shader replacements capture the original page and keep its size.
        implicitHeight: builtinVisible ? pageLoader.height : Math.max(120, pluginImplicitHeight)
        height: implicitHeight
        transform: Translate {
            id: pageShift
        }

        Loader {
            id: pageLoader
            width: pageExtension.width
            height: item ? item.implicitHeight : 0
            onStatusChanged: if (status === Loader.Error)
                console.warn("Settings page failed to load: " + view.category)
            onLoaded: {
                view.resetPosition();
                Qt.callLater(view.resetPosition);
                pageEnter.restart();
            }
        }
    }

    ParallelAnimation {
        id: pageEnter
        NumberAnimation {
            target: pageExtension
            property: "opacity"
            from: 0
            to: 1
            duration: Theme.motion
            easing.type: Easing.OutCubic
        }
        NumberAnimation {
            target: pageShift
            property: "y"
            from: 12
            to: 0
            duration: Theme.motion
            easing.type: Easing.OutCubic
        }
    }
}
