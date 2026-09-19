import QtQuick
import QtTest
import "../../qml/settings/components"
import "../../qml/style"

TestCase {
    id: test
    name: "UpdateState"
    visible: true
    when: windowShown
    width: 700
    height: 1000

    QtObject {
        id: backendShell
        property var state: ({})
        property var updateInstall: ({state: "idle"})
        property var commands: []
        function tr(source) { return source }
        function command(method, value) { commands = commands.concat([{method: method, value: value}]) }
        function openUrl(url) { command("open-url", url) }
        function installUpdate(channel, target) { command("install", channel + " " + target) }
        function rollbackUpdate() { command("rollback", "") }
    }
    UpdateState { id: model; shell: backendShell }
    UpdateCard { id: card; shell: backendShell; width: 560; height: implicitHeight }

    function init() {
        Theme.animations = false
        backendShell.commands = []
        backendShell.updateInstall = {state: "idle"}
        backendShell.state = {
            appearance: {updateChannel: "dev"},
            sessionActions: {reboot: false},
            update: {channel: "dev", status: "idle", currentVersion: "1.0.0", currentCommit: "abcdef0123456789", startedAt: 100}
        }
    }
    function setUpdate(values) {
        backendShell.state = Object.assign({}, backendShell.state, {update: Object.assign({}, backendShell.state.update, values)})
    }
    function completed(values) {
        backendShell.updateInstall = Object.assign({state: "completed", channel: "dev", target: "fedcba9876543210", updatedAt: 50, progress: 100}, values || {})
    }
    function test_idle_does_not_invent_latest_version() {
        compare(model.latestRef, "")
        compare(model.refLabel(model.latestRef), "—")
        verify(!model.canInstall)
    }
    function test_old_completion_does_not_hide_new_check() {
        completed()
        setUpdate({status: "checking"})
        compare(model.status, "checking")
        setUpdate({status: "error", error: "Network unavailable"})
        compare(model.status, "error")
        setUpdate({status: "available", latestCommit: "11111112222222"})
        compare(model.status, "available")
        verify(model.canInstall)
    }
    function test_install_completion_requires_restart_and_blocks_duplicate_install() {
        setUpdate({status: "available", latestCommit: "fedcba9876543210"})
        completed({updatedAt: 150})
        compare(model.status, "restart")
        verify(model.restartRequired)
        verify(!model.canInstall)
        setUpdate({startedAt: 200, currentCommit: "fedcba9876543210", status: "upToDate"})
        verify(!model.restartRequired)
        compare(model.status, "upToDate")
    }
    function test_switch_channel_does_not_reuse_previous_result() {
        setUpdate({status: "available", latestCommit: "fedcba9876543210"})
        backendShell.state = Object.assign({}, backendShell.state, {appearance: {updateChannel: "stable"}})
        compare(model.checkStatus, "idle")
        compare(model.latestRef, "")
        verify(!model.canInstall)
        completed({channel: "dev", updatedAt: 50})
        setUpdate({channel: "stable", status: "available", latestVersion: "v2.0.0"})
        verify(model.canInstall)
    }
    function test_rollback_history_does_not_require_restart_forever() {
        completed({rollback: true, channel: "", target: "", updatedAt: 150})
        verify(model.restartRequired)
        setUpdate({startedAt: 200})
        verify(!model.restartRequired)
    }
    function test_active_progress_and_failed_history_are_separate() {
        backendShell.updateInstall = {state: "running", progress: 37, stage: "build"}
        compare(model.status, "installing")
        compare(model.progress, 37)
        verify(!model.canInstall)
        backendShell.updateInstall = {state: "error", progress: 37, message: "Build failed"}
        setUpdate({status: "available", latestCommit: "fedcba9876543210"})
        verify(model.failed)
        verify(model.hasHistory)
        verify(model.canInstall)
        compare(model.status, "available")
    }
    function test_invalid_progress_and_unknown_commit() {
        backendShell.updateInstall = {state: "running", progress: "not a number"}
        compare(model.progress, 0)
        verify(!model.sameRef("dev", "unknown", "unknown"))
        verify(!model.sameRef("dev", "abc", "abcdef0123456789"))
        verify(model.sameRef("dev", "abcdef0", "abcdef0123456789"))
    }
    function test_card_layout() {
        card.width = 360
        setUpdate({status: "available", latestCommit: "fedcba9876543210", latestMessage: "A longer update description that should wrap within the card."})
        wait(100)
        verify(card.height > 200)
        verify(card.height < test.height)
        card.width = 560
    }
}
