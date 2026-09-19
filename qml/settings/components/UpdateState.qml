import QtQuick

QtObject {
    required property var shell
    readonly property var update: shell.state.update || ({})
    readonly property var install: shell.updateInstall && shell.updateInstall.state !== "idle"
        ? shell.updateInstall : (update.install || ({}))
    readonly property string channel: (shell.state.appearance || {}).updateChannel === "dev" ? "dev" : "stable"
    readonly property string checkStatus: update.channel === channel ? String(update.status || "idle") : "idle"
    readonly property bool installing: install.state === "running"
    readonly property bool completed: install.state === "completed"
    readonly property bool failed: install.state === "error"
    readonly property bool hasHistory: completed || failed
    readonly property real progress: isFinite(Number(install.progress)) ? Math.max(0, Math.min(100, Number(install.progress))) : 0
    readonly property string latestRef: update.channel !== channel ? "" : String(channel === "dev" ? update.latestCommit || "" : update.latestVersion || "")
    readonly property string currentRef: String(channel === "dev" ? update.currentCommit || "" : update.currentVersion || "")
    readonly property bool runningInstalledTarget: sameRef(String(install.channel || ""), String(install.target || ""),
        String(install.channel === "dev" ? update.currentCommit || "" : update.currentVersion || ""))
    readonly property bool restartRequired: completed && (Number(update.startedAt) > 0
        ? Number(install.updatedAt) >= Number(update.startedAt)
        : Boolean(install.target) && !runningInstalledTarget)
    readonly property bool latestAlreadyInstalled: completed && !install.rollback && install.channel === channel
        && sameRef(channel, String(install.target || ""), latestRef) && (restartRequired || runningInstalledTarget)
    readonly property bool canInstall: checkStatus === "available" && latestRef.length > 0 && !installing && !latestAlreadyInstalled
    readonly property string status: installing ? "installing"
        : restartRequired ? "restart"
        : checkStatus === "checking" ? "checking"
        : latestAlreadyInstalled ? "upToDate"
        : checkStatus

    function sameRef(selectedChannel, left, right) {
        if (selectedChannel === "dev") {
            return /^[a-f0-9]{7,40}$/i.test(left) && /^[a-f0-9]{7,40}$/i.test(right)
                && (left.toLowerCase().startsWith(right.toLowerCase()) || right.toLowerCase().startsWith(left.toLowerCase()))
        }
        return selectedChannel === "stable" && left.length > 0 && right.length > 0
            && left.replace(/^v/, "") === right.replace(/^v/, "")
    }

    function refLabel(ref) {
        return !ref || ref === "unknown" ? "—" : channel === "dev" ? ref.slice(0, 12) : ref
    }

    function stageLabel(stage) {
        const labels = {
            prepare: "Preparing", download: "Downloading source", verify: "Verifying update",
            dependencies: "Checking dependencies", package: "Preparing package",
            "install-script": "Running install script", configure: "Configuring build",
            build: "Building LunaDash", backup: "Creating rollback backup", install: "Installing files",
            cleanup: "Cleaning temporary source", finalize: "Finalizing", rollback: "Restoring previous installation",
            interrupted: "Interrupted", complete: "Completed"
        }
        return shell.tr(labels[stage] || "Waiting")
    }
}
