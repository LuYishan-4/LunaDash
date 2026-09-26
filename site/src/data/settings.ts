export interface SettingGroup {
  id: string;
  name: string;
  description: string;
  details: string[];
}
export const groups: SettingGroup[] = [
  {
    id: "general",
    name: "Language & everyday preferences",
    description:
      "Open General to select your language, font family or clock format, or reopen the welcome screen.",
    details: [
      "The shell updates language live; reopen native applications to apply a different language.",
      "Reset desktop preferences restores LunaDash appearance, keyboard and workspace defaults. It does not erase files or network profiles.",
    ],
  },
  {
    id: "appearance",
    name: "Color, wallpaper & soft blur",
    description:
      "Choose an accent color and wallpaper image, then tune gaps, panel height, background blur and motion. Wallpaper previews and the desktop reveal new images with a circle expanding from the bottom-right corner.",
    details: [
      "Blur starts enabled at radius 18. Lower the radius to reduce GPU work; window opacity ranges from 60% to 100%.",
      "Set animation duration between 0 and 600 ms, or disable animations for reduced motion.",
      "Plugin windows and LunaDash file dialogs follow the accent and font settings. Kitty retains your configuration; third-party applications use their own themes.",
    ],
  },
  {
    id: "windows",
    name: "Tiling & workspaces",
    description:
      "Choose 1–10 workspaces and window gaps. Windows stay within one screen: two split left/right, then new windows split the largest tile. Groups support up to eight vertical rows.",
    details: [
      "Super+1…9 and Super+0 switch workspaces (0 selects workspace 10); Super+Shift+1…9 moves the focused window to an existing workspace.",
      "Super+H/L focuses left/right; Super+K/J focuses up/down. Super+F maximizes one window or restores the saved workspace tiling, and Super+C closes the focused window.",
      "Alt + left-drag swaps window slots; drop at a top/bottom edge to insert into a column. A single window moves without resizing. Add Shift to resize shared boundaries and redistribute space without overlap.",
      "The taskbar groups windows into workspace capsules without numeric badges: the current workspace is brighter and the others use a darker tint. Selecting a tiled task animates the workspace change and maximizes it, temporarily hiding its workspace peers. Click the active task again or press Super+F to restore all saved tile sizes and positions. Alt+Tab previews windows in the current workspace. Super+Tab opens the workspace overview. Release Alt or Super to confirm the matching switcher, or press Esc to cancel.",
      "Reducing workspace count moves windows from removed workspaces to the last remaining workspace.",
    ],
  },
  {
    id: "display",
    name: "Display, keyboard & pointer",
    description:
      "Adjust internal backlight and per-monitor DDC/CI brightness, select a supported primary-output mode and scale text and controls. Keyboard settings change the Wayland keymap and repeat behavior.",
    details: [
      "Available keyboard layouts: us, gb, de, fr, es, jp and tw. Repeat rate is 0–60; delay is 200–1500 ms.",
      "Cursor size applies to the next session. Host monitor, mouse and touchpad editors are labelled as host controls.",
      "Confirm resolution, refresh-rate and scale changes within 15 seconds or they revert automatically. Physical modes in nested sessions belong to the host. External brightness requires DDC/CI and I2C access; multi-monitor arrangement and HDR remain incomplete.",
    ],
  },
  {
    id: "services",
    name: "Sound, networking & power",
    description:
      "Sound controls the default output and microphone through WirePlumber. Power profiles use power-profiles-daemon.",
    details: [
      "Volume is capped at 100%. Unavailable devices and failed commands are shown rather than reported as successful changes.",
      "Existing network connections are reused. Network configuration opens NetworkManager's editor; passwords remain in that editor. Saved profiles and Wi-Fi names that contain a colon are listed correctly.",
      "Bluetooth, printers, disks, accounts, clock and accessibility pages show available system tools and packages to install.",
      "LunaDash displays application notifications and starts an installed polkit authentication agent. Screen locking and screen-sharing/PipeWire portals remain incomplete.",
    ],
  },
  {
    id: "applications",
    name: "Default applications & startup",
    description:
      "Applications and startup lists every installed application for each default role, with the role default pinned at the top of the list.",
    details: [
      "Terminal defaults to Kitty, opened with Super+T, and respects your existing configuration. The login shell and user Fish configuration are not rewritten.",
      "Files defaults to Dolphin. Select another installed file manager in Settings; the LunaDash file picker and screen/window sharing chooser remain available.",
      "The browser role controls web links opened by LunaDash and prefers Chrome when available. Default application roles affect LunaDash launchers and shortcuts; file-type associations are configured separately.",
      "Startup selection currently covers built-in tools; general desktop-entry autostart and session restoration are not implemented.",
    ],
  },
];
