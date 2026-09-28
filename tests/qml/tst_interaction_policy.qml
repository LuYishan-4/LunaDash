import QtQuick
import QtTest
import "../../qml/components/PopupPolicy.js" as Popups
import "../../qml/plugins/ActivationChanges.js" as Activation

TestCase {
    name: "InteractionPolicy"
    function test_popupToggleAndDismiss() {
        const shell = {overviewOpen: false,
            setAppearance: function(value) { this.overviewOpen = value.overview }}
        Object.keys(Popups.properties).forEach(name => shell[Popups.properties[name]] = false)
        Object.keys(Popups.properties).forEach(name => {
            Popups.toggle(shell, name)
            compare(shell[Popups.properties[name]], true)
            Popups.toggle(shell, name)
            compare(shell[Popups.properties[name]], false)
        })
        Popups.toggle(shell, "settings")
        Popups.toggle(shell, "audio")
        Popups.toggle(shell, "overview")
        Popups.dismiss(shell, "audio")
        compare(shell.settingsOpen, false)
        compare(shell.volumePopupOpen, true)
        compare(shell.overviewOpen, false)
        Popups.dismiss(shell, "")
        compare(shell.volumePopupOpen, false)
    }
    function test_pluginToggleDiffs() {
        const disabled = {installed: [{id: "one", target: "panel", enabled: false}]}
        const enabled = {installed: [{id: "one", target: "panel", enabled: true}]}
        compare(Activation.changes(disabled, enabled).length, 1)
        compare(Activation.changes(disabled, enabled)[0].enabled, true)
        compare(Activation.changes(enabled, disabled)[0].enabled, false)
        compare(Activation.changes(enabled, enabled).length, 0)
        compare(Activation.changes({}, disabled).length, 0)
        const multi = {installed: [
            {id: "one", target: "panel", enabled: true},
            {id: "one", target: "launcher", enabled: true}]}
        compare(Activation.changes(disabled, multi).length, 1)
        const switched = {installed: [{id: "two", target: "panel", enabled: true}]}
        compare(Activation.changes(enabled, switched).length, 2)
        compare(Activation.changes(multi, {installed: multi.installed.slice().reverse()}).length, 0)
    }
}
