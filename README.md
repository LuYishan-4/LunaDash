<div align="center">

<a href="https://luyishan-4.github.io/LunaDash/">
  <img src="docs/brand/banner.svg" alt="LunaDash crescent moon logo and wordmark" width="880">
</a>

### a desktop environment.

A Wayland desktop that brings everyday essentials together, with room to make it your own.<br>
**Our goal: a useful first login, without giving up customization.**

<p>
  <a href="#linux-distributions"><img src="https://img.shields.io/badge/status-development_preview-d3bfe6?style=flat-square" alt="Development preview"></a>
  <a href="https://github.com/LuYishan-4/LunaDash/actions/workflows/main-build.yml?query=branch%3Adev"><img src="https://github.com/LuYishan-4/LunaDash/actions/workflows/main-build.yml/badge.svg?branch=dev" alt="Build on dev"></a>
  <a href="https://github.com/LuYishan-4/LunaDash/pulls"><img src="https://img.shields.io/github/issues-pr/LuYishan-4/LunaDash?style=flat-square&amp;label=pull%20requests&amp;color=9ccbfb" alt="Open pull requests"></a>
  <a href="https://github.com/LuYishan-4/LunaDash/issues"><img src="https://img.shields.io/github/issues/LuYishan-4/LunaDash?style=flat-square&amp;color=d3bfe6" alt="Open issues"></a>
  <a href="LICENSE"><img src="https://img.shields.io/badge/license-GPL--3.0--only-9ccbfb?style=flat-square" alt="GPL-3.0-only license"></a>
</p>

**[<kbd> Install </kbd>](#install)** · **[<kbd> Gallery </kbd>](#gallery)** · **[<kbd> Customize </kbd>](docs/CONFIGURATION.md)** · **[<kbd> Contribute </kbd>](#contribute)**

[Website](https://luyishan-4.github.io/LunaDash/) · [Documentation](#documentation) · [Releases](https://github.com/LuYishan-4/LunaDash/releases)

</div>

## A starting point, with possibilities

- **Everyday essentials, together.** A panel, launcher, dashboard, notifications, settings and file tools are part of the desktop.
- **A workspace that moves with you.** Scrollable window columns, grouped and floating windows, keyboard controls and window animations.
- **Make the defaults yours.** Change wallpaper, colors, spacing, shortcuts and default applications from Settings.
- **Go further when you want.** Arrange shell modules, customize Quickshell/QML and use the local control interface for your own workflow.

Built with **C++20 · C11 · wlroots · OpenGL · Qt 6 · Quickshell/QML**. LunaDash is an active development preview, with Arch Linux as its primary platform.

## Gallery

<table>
  <tr>
    <td width="50%" align="center"><a href="docs/image/LunaDash-20260920-024312-792.png"><img src="docs/image/LunaDash-20260920-024312-792.png" alt="LunaDash dashboard with a wallpaper preview, clock, system status and application shortcuts" width="440"></a><sub>The dashboard brings everyday controls together.</sub></td>
    <td width="50%" align="center"><a href="docs/image/LunaDash-20260920-024259-008.png"><img src="docs/image/LunaDash-20260920-024259-008.png" alt="LunaDash Appearance settings showing wallpaper selection and the accent color picker" width="440"></a><sub>Personalize the look from one settings center.</sub></td>
  </tr>
  <tr>
    <td align="center"><a href="docs/image/LunaDash-20260920-024708-948.png"><img src="docs/image/LunaDash-20260920-024708-948.png" alt="LunaDash workspace with a terminal beside grouped Discord and Zed windows" width="440"></a><sub>Keep your tools together in scrollable columns.</sub></td>
    <td align="center"><a href="docs/image/LunaDash-20260920-024455-959.png"><img src="docs/image/LunaDash-20260920-024455-959.png" alt="LunaDash desktop with the calendar panel and its customized header image" width="440"></a><sub>From the wallpaper to the calendar image.</sub></td>
  </tr>
</table>

<sub>Actual desktop screenshots from September 20, 2026. Click any image for the original. Layout and appearance are configurable.</sub>

## Install

Clone the development branch, then run the session installer:

```sh
git clone --branch dev https://github.com/LuYishan-4/LunaDash.git
cd LunaDash
./scripts/install-session.sh
```

The installer handles distribution dependencies, builds LunaDash and installs the login session. Log out and choose **LunaDash** in your display manager. The first-run guide helps you choose your language and personalize the desktop.

Use `./scripts/install-session.sh --dry-run` to preview the steps. The shell requires **Quickshell 0.3+**; if your distribution does not package it, the installer reports the missing dependency. See the [installation guide](docs/LOGIN_SESSION.md) for options and recovery, or [build and testing](docs/TESTING_AND_FILES.md) for manual builds and nested sessions.

## Linux distributions

| Distribution | Current coverage |
| --- | --- |
| [![Arch Linux](https://img.shields.io/badge/Arch_Linux-1793D1?logo=archlinux&logoColor=white&style=flat-square)](https://archlinux.org) | Primary development platform; source-build CI; pacman/makepkg installation |
| [![Ubuntu 24.04](https://img.shields.io/badge/Ubuntu_24.04-E95420?logo=ubuntu&logoColor=white&style=flat-square)](https://ubuntu.com) | Main build and runtime CI; apt dependency support |
| [![Debian 13](https://img.shields.io/badge/Debian_13-A81D33?logo=debian&logoColor=white&style=flat-square)](https://www.debian.org) | Source-build CI; apt dependency support |
| [![Fedora 45](https://img.shields.io/badge/Fedora_45-51A2DA?logo=fedora&logoColor=white&style=flat-square)](https://fedoraproject.org) | Source-build CI; dnf dependency support |
| [![openSUSE Tumbleweed](https://img.shields.io/badge/openSUSE_Tumbleweed-73BA25?logo=opensuse&logoColor=white&style=flat-square)](https://www.opensuse.org) | Source-build CI; zypper dependency support |
| [![Alpine Edge](https://img.shields.io/badge/Alpine_Edge-0D597F?logo=alpinelinux&logoColor=white&style=flat-square)](https://alpinelinux.org) | Source-build CI; apk dependency support |
| [![Void Linux](https://img.shields.io/badge/Void_Linux-478061?logo=voidlinux&logoColor=white&style=flat-square)](https://voidlinux.org) | Installer support; outside the current distribution CI matrix |
| [![Gentoo](https://img.shields.io/badge/Gentoo-54487A?logo=gentoo&logoColor=white&style=flat-square)](https://www.gentoo.org) | Installer support; outside the current distribution CI matrix |

Other distributions can use the standard CMake installation once dependencies are available. See [Actions](https://github.com/LuYishan-4/LunaDash/actions?query=branch%3Adev) for results on each commit. Build and software-rendering checks do not establish support for every GPU, monitor or standalone session. Screen locking and screen-sharing/PipeWire portals remain incomplete. Native plugins are disabled by default and run without a sandbox when enabled.

## A few shortcuts

| Shortcut | Action |
| --- | --- |
| `Super` + `Return` / `E` / `D` | Terminal / Files / launcher |
| `Super` + `H` / `L` | Focus the left / right column |
| `Super` + `K` / `J` | Focus another window in the column |
| `Super` + `Space` | Toggle floating |
| `Super` + `Shift` + `S` | Select a screenshot region |
| `Super` + `1`–`9` | Switch workspace |

Change bindings in **Settings → Keyboard shortcuts**. See [settings](docs/SETTINGS.md) for grouping, resizing and control commands.

## Documentation

| Start here | Make it yours |
| --- | --- |
| [Install and run](docs/LOGIN_SESSION.md) | [Appearance and configuration](docs/CONFIGURATION.md) |
| [Build and test](docs/TESTING_AND_FILES.md) | [Settings and shortcuts](docs/SETTINGS.md) |
| [Display, DDC/CI and startup](docs/DISPLAY_AND_STARTUP.md) | [Shell modules](docs/MODULES.md) |
| [Region screenshots](docs/SCREEN_CAPTURE.md) | [Default apps and Files](docs/DEFAULT_APPS_AND_FILES.md) |
| [Source architecture](docs/ARCHITECTURE.md) | [Plugin interfaces](docs/PLUGINS.md) |

## Contribute

Help make the default experience useful and customization approachable. Bug reports, design feedback, documentation and code contributions are welcome.

Send pull requests to **`dev`**. Use a title that says what changes, explain the user-visible result, See [CONTRIBUTING.md](CONTRIBUTING.md) and the [PR template](.github/pull_request_template.md).

---

<p align="center">
  <img src="docs/brand/icon.svg" alt="" width="32"><br>
  <strong>LunaDash</strong> · Ready to use. Yours to shape.<br>
  <a href="LICENSE">GPL-3.0-only</a>
</p>
