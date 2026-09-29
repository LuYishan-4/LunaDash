# NixOS

LunaDash **1.0.1a** includes a Nix flake, package and NixOS module. Arch Linux remains the primary development platform. This is a development preview; keep another working session available.

## Add the session

Use the flake in an existing NixOS configuration with flakes enabled:

```nix
{
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    lunadash.url = "github:LuYishan-4/LunaDash/dev";
    lunadash.inputs.nixpkgs.follows = "nixpkgs";
  };

  outputs = { nixpkgs, lunadash, ... }: {
    nixosConfigurations.my-host = nixpkgs.lib.nixosSystem {
      system = "x86_64-linux";
      modules = [
        ./configuration.nix
        lunadash.nixosModules.default
        { programs.lunadash.enable = true; }
      ];
    };
  };
}
```

Apply your configuration with `sudo nixos-rebuild switch --flake .#my-host`, then log out and select **LunaDash (Wayland)** in your display manager. The module registers the session; it does not choose or enable a display manager. For an SDDM host, configure `services.displayManager.sddm.enable = true` in your system configuration. A manual TTY login can run `lunadash-session` after PAM/logind has created a user runtime directory.

The package targets `x86_64-linux` and `aarch64-linux`. CI builds and exercises x86_64; aarch64 is evaluated only. Following your host's nixpkgs uses its Qt and wlroots packages; the repository's committed lock is the reference used by CI. Older stable nixpkgs releases are not covered. The package uses wlroots 0.20 for its foreign-toplevel capture API and Quickshell 0.3+, with Qt multimedia and SVG support included.

## Source layout

The repository root contains `flake.nix` and `flake.lock`. Nix implementation files live in `nix/`: `package.nix` owns the derivation and runtime wrappers, `module.nix` owns NixOS options and services, and `checks.nix` owns package/module/runtime verification.

For wlroots 0.19+, CMake generates image-copy capture declarations from the installed Wayland protocol XML. The build does not depend on distributions shipping that generated header.

## What the module configures

- The LunaDash package, display-manager session entries, graphics support, polkit, DConf and optional XWayland compatibility.
- Kitty and Dolphin for the default application roles. Set `programs.lunadash.enableDefaultApps = false` to choose your own applications.
- The LunaDash FileChooser/Settings portal, wlr ScreenCast/Screenshot backend and GTK fallback, including D-Bus and systemd user activation files.
- The LunaDash screen-sharing chooser configuration under `/etc/xdg/xdg-desktop-portal-wlr/LunaDash`.

Networking, Bluetooth, PipeWire, WirePlumber, input methods, fonts and the display manager remain host choices. Screen sharing and audio require PipeWire; enable `services.pipewire.enable = true` and configure the services your host needs. For Fcitx, use NixOS's `i18n.inputMethod` options and install the desired language engines. Installing the package alone does not enable these system services.

The package keeps executables, QML, assets and portal metadata in the Nix store. Wrappers supply executable and Qt module search paths. Native plugins stay disabled by default; metadata validation is not isolation.

## Build, develop and update

```sh
nix build .#lunadash
nix develop
nix flake check --print-build-logs --no-update-lock-file
```

These are commands for developers; they do not enable the NixOS session. The flake also exports `overlays.default` and `nixosModules.lunadash`. The module's `programs.lunadash.package` option can select an overridden package.

Update the `lunadash` input in your host flake, then rebuild the system:

```sh
nix flake update lunadash
sudo nixos-rebuild switch --flake .#my-host
# Restore a previous system generation if needed:
sudo nixos-rebuild switch --rollback
```

Use NixOS generations for installation, updates and rollback. `install.sh`, `install-session.sh` and the desktop source updater are not NixOS installation paths. The session/dependency installers reject NixOS; the packaged updater reports the NixOS update command without requesting privilege escalation or writing system files.

## Verification limits

The Nix CI suite evaluates module assertions and session/portal registration, builds the installed package and starts its compositor with headless wlroots/pixman using the installed wrappers. Its state JSON is retained as a CI artifact. This does not verify an actual display-manager login, Quickshell visuals, physical input, GPU drivers, multi-monitor behavior or PipeWire capture on NixOS. Those need a real session and screenshots before release qualification. See [CI](CI.md) and [testing](TESTING_AND_FILES.md).

Packaging follows the upstream [Qt wrapper guidance](https://github.com/NixOS/nixpkgs/blob/master/doc/languages-frameworks/qt.section.md) and [NixOS portal module](https://github.com/NixOS/nixpkgs/blob/master/nixos/modules/config/xdg/portal.nix).

Source archives produced by `scripts/make-source.sh` also include the flake, lock and `nix/` definitions.
