# LunaDash documentation — English

[Traditional Chinese](../zh/README.md) · [Documentation index](../README.md)

These guides describe the current `dev` branch. LunaDash remains a development preview; nested and CI results do not establish compatibility with every physical GPU, display manager, input device, application, or multi-monitor setup.

## Start here

- [Boot, install and recovery](LOGIN_SESSION.md)
- [Settings, shortcuts and desktop behavior](SETTINGS.md)
- [Build, test and source map](TESTING_AND_FILES.md)
- [Source architecture](ARCHITECTURE.md)

## Desktop behavior

- [First-run setup and configuration](CONFIGURATION.md)
- [Display controls and startup](DISPLAY_AND_STARTUP.md)
- [Window interactions](WINDOWS.md)
- [Window layout templates](WINDOW_LAYOUT_TEMPLATES.md)
- [Languages and input methods](INPUT_METHODS.md)
- [XWayland compatibility](XWAYLAND.md)
- [Window motion and effects](EFFECTS.md)
- [Screen capture](SCREEN_CAPTURE.md)
- [Media panel](MEDIA.md)
- [Shell rendering](SHELL_RENDERING.md)

## Extending LunaDash

- [Shell modules](MODULES.md)
- [Plugin SDK 2](PLUGINS.md)
- [Plugin target reference](PLUGIN_TARGETS.md)
- [C core and C++ integration](C_CORE.md)
- [Graphics and renderer resources](GRAPHICS.md)

## Files, maintenance and project process

- [Files and file associations](FILES.md)
- [Default applications and Files](DEFAULT_APPS_AND_FILES.md)
- [File-association migration](FILE_ASSOCIATION_MIGRATION.md)
- [Interface translations](TRANSLATIONS.md)
- [Security and crash checks](SECURITY_CHECKS.md)
- [Release process](RELEASE_PROCESS.md)
- [Website and GitHub Pages](WEBSITE.md)
- [Testing shortcut](TESTING.md)

## Current dev snapshot

The current documentation covers the wlroots compositor and lifecycle split, bounded tiling plus the opt-in stacking strategy, a 2×5 ten-workspace switcher, Plugin SDK 2 (native C/C++, Quickshell and OpenGL packages), MPRIS media controls, GPU/software wallpaper transitions, brightness and DDC/CI controls, region screenshots through slurp/grim, and XWayland helpers that can be prepared without forcing a native Wayland application onto X11.
