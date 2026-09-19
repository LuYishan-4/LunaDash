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
      "Choose a color swatch, an image or shader wallpaper, then tune gaps, panel height, background blur and motion.",
    details: [
      "Blur starts enabled at radius 18. Lower the radius to reduce GPU work; window opacity ranges from 60% to 100%.",
      "Set animation duration between 0 and 600 ms, or disable animations for reduced motion.",
      "Files follows the accent immediately. New Konsole terminals receive the generated palette; third-party apps keep their own themes.",
    ],
  },
  {
    id: "windows",
    name: "Tiling & workspaces",
    description:
      "Choose 1–9 workspaces, a 30–70% master pane and whether new windows start floating.",
    details: [
      "Super+1…9 switches workspaces; Super+Shift+1…9 moves the focused window to an existing workspace.",
      "Super+J/K focuses, Super+H/L changes the master ratio, Super+Space toggles floating, and Super+Q closes the focused window.",
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
      "Locking, a complete notification service, polkit agent and full portal integration remain incomplete.",
    ],
  },
  {
    id: "applications",
    name: "Your terminal & file manager",
    description:
      "Applications and startup lists every installed application for each default role, with the built-in default pinned at the top of the list.",
    details: [
      "Terminal defaults to Konsole with interactive Fish and a LunaDash prompt. The login shell and user Fish configuration are not rewritten.",
      "Files defaults to the LunaDash file manager. Choose icon or details view, browse common locations, filter a folder, and use the compact toolbar.",
      "These defaults affect LunaDash launchers and shortcuts, not system-wide MIME associations.",
      "Startup selection currently covers built-in tools; general desktop-entry autostart and session restoration are not implemented.",
    ],
  },
];
