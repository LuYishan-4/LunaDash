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
      "Choose an accent color and wallpaper image, then tune gaps, panel height, background blur and motion.",
    details: [
      "Blur starts enabled at radius 18. Lower the radius to reduce GPU work; window opacity ranges from 60% to 100%.",
      "Set animation duration between 0 and 600 ms, or disable animations for reduced motion.",
      "Built-in Files, plugin windows and LunaDash file dialogs follow the accent and font settings. Konsole retains your selected profile; third-party applications use their own themes.",
    ],
  },
  {
    id: "windows",
    name: "Tiling & workspaces",
    description:
      "Choose 1–9 workspaces, a default column width from 30% to 70%, and whether new windows start floating.",
    details: [
      "Super+1…9 switches workspaces; Super+Shift+1…9 moves the focused window to an existing workspace.",
      "Super+H/L focuses the left/right column; Super+K/J focuses windows within a column. Super+Space toggles floating, Super+F maximizes or restores, and Super+Q closes the focused window.",
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
      "Existing network connections are reused. Network configuration opens NetworkManager's editor; passwords remain in that editor.",
      "Bluetooth, printers, disks, accounts, clock and accessibility pages show available system tools and packages to install.",
      "LunaDash displays application notifications and starts an installed polkit authentication agent. Screen locking and screen-sharing/PipeWire portals remain incomplete.",
    ],
  },
  {
    id: "applications",
    name: "Default applications & startup",
    description:
      "Applications and startup lists every installed application for each default role, with the built-in default pinned at the top of the list.",
    details: [
      "Terminal defaults to Konsole and respects your existing profile, including a configured Flatpak Konsole profile. The login shell and user Fish configuration are not rewritten.",
      "Files defaults to the LunaDash file manager. Choose icon or details view, browse common locations, filter a folder, and use the compact toolbar.",
      "The browser role controls web links opened by LunaDash and prefers Chrome when available. Default application roles affect LunaDash launchers and shortcuts; file-type associations are configured separately.",
      "Startup selection currently covers built-in tools; general desktop-entry autostart and session restoration are not implemented.",
    ],
  },
];
