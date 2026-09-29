{ pkgs, package, system, nixpkgs, module }:
let
  host = nixpkgs.lib.nixosSystem {
    inherit system;
    modules = [ module ({ ... }: {
      programs.lunadash.enable = true;
      boot.loader.grub.enable = false;
      fileSystems."/" = { device = "/dev/vda"; fsType = "ext4"; };
      system.stateVersion = "26.05";
    }) ];
  };
  config = host.config;
  failures = builtins.filter (item: !item.assertion) config.assertions;
  moduleCheck = assert failures == [ ];
    assert builtins.elem package config.services.displayManager.sessionPackages;
    assert builtins.elem package config.services.dbus.packages;
    assert builtins.elem package config.systemd.packages;
    assert config.xdg.portal.config.lunadash."org.freedesktop.impl.portal.FileChooser" == "lunadash";
    assert config.security.polkit.enable && config.hardware.graphics.enable;
    pkgs.writeText "lunadash-module.json" (builtins.toJSON {
      sessions = package.providedSessions;
      portals = config.xdg.portal.config.lunadash;
      sessionData = "${config.services.displayManager.sessionData.desktops}";
      portalConfig = config.environment.etc."xdg/xdg-desktop-portal-wlr/LunaDash".source;
    });
in {
  inherit package;
  module = moduleCheck;
  runtime = pkgs.runCommand "lunadash-installed-runtime" {
    nativeBuildInputs = [ pkgs.python3 pkgs.dbus ];
  } ''
    export HOME="$TMPDIR/home"
    export XDG_RUNTIME_DIR="$TMPDIR/runtime"
    export XDG_CONFIG_HOME="$HOME/.config"
    mkdir -p "$HOME" "$XDG_RUNTIME_DIR" "$XDG_CONFIG_HOME"
    chmod 700 "$XDG_RUNTIME_DIR"
    test -r ${package}/share/wayland-sessions/lunadash.desktop
    test -r ${package}/share/lunadash/shell/shell.qml
    test -r ${package}/share/xdg-desktop-portal/portals/lunadash.portal
    grep -F '${package}/libexec/xdg-desktop-portal-lunadash' \
      ${package}/share/dbus-1/services/org.freedesktop.impl.portal.desktop.lunadash.service
    ${package}/bin/lunadash-session --check
    export WLR_BACKENDS=headless WLR_HEADLESS_OUTPUTS=1 WLR_RENDERER=pixman
    export LUDASH_DISABLE_XWAYLAND=1 LUDASH_SKIP_SETUP=1 QT_QPA_PLATFORM=offscreen
    dbus-run-session -- ${package}/bin/lunadash-compositor \
      --no-shell --socket lunadash-nix-check --exit-after 2500 --state "$TMPDIR/state.json"
    python3 - "$TMPDIR/state.json" <<'PY'
import json, sys
state = json.load(open(sys.argv[1]))
assert not state.get("processFailure", True), state
assert state["input"]["backend"] == "wlroots", state
assert state["display"]["width"] > 0, state
PY
    mkdir -p "$out"
    cp "$TMPDIR/state.json" "$out/state.json"
  '';
}
