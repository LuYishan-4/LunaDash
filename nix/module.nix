{ config, lib, pkgs, ... }:
let
  cfg = config.programs.lunadash;
in {
  options.programs.lunadash = {
    enable = lib.mkEnableOption "the LunaDash Wayland desktop (development preview)";
    package = lib.mkOption {
      type = lib.types.package;
      description = "LunaDash package supplied by the flake or an overlay.";
    };
    enableDefaultApps = lib.mkOption {
      type = lib.types.bool;
      default = true;
      description = "Install Kitty and Dolphin for the default terminal and files roles.";
    };
  };

  config = lib.mkIf cfg.enable {
    environment.systemPackages = [ cfg.package pkgs.kdePackages.polkit-kde-agent-1 ]
      ++ lib.optionals cfg.enableDefaultApps [ pkgs.kitty pkgs.kdePackages.dolphin ];
    services.displayManager.sessionPackages = [ cfg.package ];
    services.graphical-desktop.enable = true;
    hardware.graphics.enable = true;
    security.polkit.enable = true;
    programs.dconf.enable = lib.mkDefault true;
    programs.xwayland.enable = lib.mkDefault true;

    xdg.portal = {
      enable = true;
      extraPortals = [ cfg.package pkgs.xdg-desktop-portal-gtk pkgs.xdg-desktop-portal-wlr ];
      config.lunadash = {
        default = [ "gtk" ];
        "org.freedesktop.impl.portal.FileChooser" = [ "lunadash" ];
        "org.freedesktop.impl.portal.Settings" = [ "lunadash" ];
        "org.freedesktop.impl.portal.ScreenCast" = [ "wlr" ];
        "org.freedesktop.impl.portal.Screenshot" = [ "wlr" ];
      };
    };
    environment.etc."xdg/xdg-desktop-portal-wlr/LunaDash".source =
      "${cfg.package}/etc/xdg/xdg-desktop-portal-wlr/LunaDash";
    # The portal module registers package D-Bus and systemd user services.
    # Keep PipeWire, network management, input methods and the display manager
    # under the host configuration's control.
  };
}
