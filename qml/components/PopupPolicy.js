// Pure interaction policy; popup ownership stays with the shell.
var properties = {
    launcher: "launcherOpen", orbit: "orbitOpen", settings: "settingsOpen",
    calendar: "calendarOpen", devices: "usbPopupOpen", audio: "volumePopupOpen",
    network: "wifiPopupOpen", clipboard: "clipboardPopupOpen", session: "logoutOpen",
    menu: "menuOpen", compatibility: "x11Open", wallpaper: "wallpaperGalleryOpen"
};
function dismiss(shell, except) {
    Object.keys(properties).forEach(name => {
        if (name !== except && shell[properties[name]])
            shell[properties[name]] = false;
    });
    if (except !== "overview" && shell.overviewOpen)
        shell.setAppearance({overview: false});
}
function toggle(shell, name) {
    if (name === "overview")
        shell.setAppearance({overview: !shell.overviewOpen});
    else if (properties[name])
        shell[properties[name]] = !shell[properties[name]];
}
