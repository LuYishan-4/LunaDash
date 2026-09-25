<div align="center">
<a href="https://luyishan-4.github.io/LunaDash/"><img src="docs/brand/banner.svg" alt="LunaDash" width="880"></a>

### A desktop environment.

A Wayland desktop that brings everyday essentials together, with room to make it your own.<br>
**Our goal: a useful first login, without giving up customization.**

<p>
  <a href="#linux-distributions"><img src="https://img.shields.io/badge/status-development_preview-d3bfe6?style=flat-square" alt="Development preview"></a>
  <a href="https://github.com/LuYishan-4/LunaDash/actions/workflows/main-build.yml?query=branch%3Adev"><img src="https://github.com/LuYishan-4/LunaDash/actions/workflows/main-build.yml/badge.svg?branch=dev" alt="Build on dev"></a>
  <a href="https://github.com/LuYishan-4/LunaDash/pulls"><img src="https://img.shields.io/github/issues-pr/LuYishan-4/LunaDash?style=flat-square&amp;label=pull%20requests&amp;color=9ccbfb" alt="Open pull requests"></a>
  <a href="https://github.com/LuYishan-4/LunaDash/issues"><img src="https://img.shields.io/github/issues/LuYishan-4/LunaDash?style=flat-square&amp;color=d3bfe6" alt="Open issues"></a>
  <a href="LICENSE"><img src="https://img.shields.io/badge/license-GPL--3.0--only-9ccbfb?style=flat-square" alt="GPL-3.0-only license"></a>
</p>

<a id="linux-distributions"></a>

[![Arch Linux](https://img.shields.io/badge/Arch_Linux-1793d1?logo=arch-linux&logoColor=white&style=flat-square)](https://archlinux.org)
[![Fedora](https://img.shields.io/badge/Fedora-51A2DA?logo=fedora&logoColor=white&style=flat-square)](https://fedoraproject.org)
[![Ubuntu](https://img.shields.io/badge/Ubuntu_Rolling-E95420?logo=ubuntu&logoColor=white&style=flat-square)](https://ubuntu.com)

</div>

[English](README.md) · [繁體中文](docs/readme/README.zh-TW.md) · [简体中文](docs/readme/README.zh-CN.md) · [日本語](docs/readme/README.ja.md)

## LunaDash

- **Everyday essentials, together.** A panel, launcher, card-based dashboard, resizable settings center, notifications and LunaDash-styled file/portal pickers are part of the desktop.
- **A workspace that moves with you.** Tiles contained within one screen, groups of up to eight windows, Alt dragging, a ten-workspace thumbnail overview and a taskbar spanning all workspaces.
- **Make the defaults yours.** Change wallpaper, colors, spacing, shortcuts and default applications from Settings.
- **Go further when you want.** Arrange shell modules, customize Quickshell/QML and use the local control interface for your own workflow.

Built with **C++20 · C11 · wlroots · OpenGL · Qt 6 · Quickshell/QML**. The current development version is **1.0.1a**. LunaDash is an active development preview, with Arch Linux as its primary platform.

<a id="gallery"></a>

## Gallery

<table>
  <tr>
    <td width="50%" align="center"><a href="docs/image/LunaDash-20260920-024312-792.png"><img src="docs/image/LunaDash-20260920-024312-792.png" alt="The dashboard brings together the clock, system status and everyday controls." width="440"></a></td>
    <td width="50%" align="center"><a href="docs/image/LunaDash-20260920-024259-008.png"><img src="docs/image/LunaDash-20260920-024259-008.png" alt="Choose wallpaper and accent colors in Appearance settings." width="440"></a></td>
  </tr>
  <tr>
    <td width="50%" align="center"><a href="docs/image/LunaDash-20260920-024708-948.png"><img src="docs/image/LunaDash-20260920-024708-948.png" alt="Use the terminal, Discord and Zed in tiled windows." width="440"></a></td>
    <td width="50%" align="center"><a href="docs/image/LunaDash-20260920-024455-959.png"><img src="docs/image/LunaDash-20260920-024455-959.png" alt="The calendar panel also supports custom images." width="440"></a></td>
  </tr>
</table>

<sub>Actual desktop screenshots from September 20, 2026. Click any image for the original. Layout and appearance are configurable.</sub>

<a id="install"></a>

## Install

On Arch Linux, the guided installer offers Traditional Chinese / English,
optional applications and a separate system-locale choice. Review the script
before executing downloaded code. See the [English guide](docs/en/GUIDED_INSTALL.md)
or [繁體中文指南](docs/zh/GUIDED_INSTALL.md).

```bash
curl -fsSL --connect-timeout 10 https://raw.githubusercontent.com/LuYishan-4/LunaDash/dev/install.sh | bash
```

For developer/testing installs or other distributions, the existing session
installer remains available and unchanged:

```sh
git clone --branch dev https://github.com/LuYishan-4/LunaDash.git
cd LunaDash
./scripts/install-session.sh
```

The installer handles distribution dependencies, builds LunaDash and installs the login session. Log out and choose **LunaDash** in your display manager. The first launch opens the 1.0.1a Welcome experience with system cards and quick actions. Use the resizable Settings center to choose your language and personalize the desktop.

Use `./scripts/install-session.sh --dry-run` to preview the steps. The shell requires **Quickshell 0.3+**; if your distribution does not package it, the installer reports the missing dependency. See the [installation guide](docs/en/LOGIN_SESSION.md) for options and recovery, or [build and testing](docs/en/TESTING_AND_FILES.md) for manual builds and nested sessions.

## A few shortcuts

| Shortcut | Action |
| --- | --- |
| `Super` + `Return` / `E` / `D` | Terminal / Files / launcher |
| `Super` + `T` | Open Kitty (default terminal) |
| `Super` + `H` / `L` | Focus the window to the left / right |
| `Super` + `K` / `J` | Focus the window above / below |
| `Super` + `F` | Maximize one window / restore all workspace tiles |
| `Alt` + `Tab` | Preview windows in the current workspace; release Alt to select |
| `Super` + `Tab` | Workspace previews; release Super to switch |
| `Super` + `W` | Toggle the directory-based wallpaper gallery |
| `Alt` + drag | Move or swap slots; add Shift to resize |
| `Super` + `Shift` + `S` | Select a screenshot region |
| `Super` + `1`–`9` / `0` | Switch workspace |

`Super` is the Meta key. Change bindings in **Settings → Keyboard shortcuts**. See [settings](docs/en/SETTINGS.md) for grouping, resizing and control commands.

<a id="documentation"></a>

## Documentation

[English docs](docs/en/README.md) · [Traditional Chinese docs](docs/zh/README.md)


| Start here | Make it yours |
| --- | --- |
| [Install and run](docs/en/LOGIN_SESSION.md) | [Appearance and configuration](docs/en/CONFIGURATION.md) |
| [Build and test](docs/en/TESTING_AND_FILES.md) | [Settings and shortcuts](docs/en/SETTINGS.md) |
| [Display, DDC/CI and startup](docs/en/DISPLAY_AND_STARTUP.md) | [Shell modules](docs/en/MODULES.md) |
| [Region screenshots](docs/en/SCREEN_CAPTURE.md) | [Default apps and Files](docs/en/DEFAULT_APPS_AND_FILES.md) |
| [Source architecture](docs/en/ARCHITECTURE.md) | [Plugin interfaces](docs/en/PLUGINS.md) |

<a id="contribute"></a>

## Contribute

Help make the default experience useful and customization approachable. Bug reports, design feedback, documentation and code contributions are welcome.

Send pull requests to **`dev`**. Use a clear title describing the change, and explain the user-visible result, the checks you actually ran, and whether the change affects default behavior or optional customization. Update the relevant **`docs/` documentation and website content**.

**PRs must not add, modify, delete, or rename files under `.github/workflows/`, release Markdown (`.md`/`.mdx`) under `site/src/pages/releases/`, or `site/src/data/releases.json`.** Describe release impact in the PR body; website release notes are generated from GitHub Releases. See the [contribution guide](CONTRIBUTING.md) and [PR template](.github/pull_request_template.md).

---

<p align="center"><img src="docs/brand/icon.svg" alt="" width="32"><br><strong>LunaDash</strong> · This project is still immature. Issue reports and PRs are welcome.<br><a href="LICENSE">GPL-3.0-only</a></p>

### Build your own desktop features

[Plugin SDK 2](docs/en/PLUGINS.md) provides C/C++ hooks, Quickshell components and OpenGL shader templates, with categorized settings and live replacement or augmentation. Start with the [target reference](docs/en/PLUGIN_TARGETS.md) or the disabled-by-default [stacking windows example](examples/plugins/stacking-windows/README.md).
