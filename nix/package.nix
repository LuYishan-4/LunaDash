{ lib, stdenv, runCommand, cmake, ninja, pkg-config, python3, makeWrapper
, qt6, wayland, wayland-protocols, wayland-scanner, wlroots_0_20
, libxkbcommon, libGL, glib, libinput, pixman, libdrm, libxcb, systemd, dbus, quickshell
, coreutils, bash, gnused, xwayland, xdg-utils, wl-clipboard, grim, slurp
, brightnessctl, ddcutil, wireplumber, pulseaudio, kdePackages
, xdg-desktop-portal-wlr, revision ? "unknown"
}:
let
  # Quickshell must inherit the QML modules imported by LunaDash, including
  # QtMultimedia, rather than relying on a user's profile or global Qt paths.
  shell = runCommand "lunadash-quickshell" {
    nativeBuildInputs = [ makeWrapper ];
  } ''
    mkdir -p "$out/bin"
    makeWrapper ${lib.getExe quickshell} "$out/bin/quickshell" \
      --prefix NIXPKGS_QT6_QML_IMPORT_PATH : "${lib.makeSearchPath qt6.qtbase.qtQmlPrefix [ qt6.qtmultimedia qt6.qt5compat ]}" \
      --prefix QT_PLUGIN_PATH : "${lib.makeSearchPath qt6.qtbase.qtPluginPrefix [ qt6.qtmultimedia ]}"
  '';
  runtimePath = lib.makeBinPath [
    shell coreutils bash gnused dbus systemd xwayland xdg-utils wl-clipboard
    grim slurp brightnessctl ddcutil wireplumber pulseaudio
  ] + ":${xdg-desktop-portal-wlr}/libexec:${kdePackages.polkit-kde-agent-1}/libexec";
in
stdenv.mkDerivation {
  pname = "lunadash";
  version = "1.0.1a";
  src = lib.fileset.toSource {
    root = ../.;
    fileset = lib.fileset.unions [
      ../CMakeLists.txt ../cmake ../src ../qml ../data
      ../protocols ../scripts ../templates ../tests ../LICENSE
      ../docs/PLUGIN_TARGETS.md ../docs/PLUGINS.md
    ];
  };

  nativeBuildInputs = [ cmake ninja pkg-config python3 makeWrapper wayland-scanner qt6.wrapQtAppsHook ];
  buildInputs = [
    qt6.qtbase qt6.qtdeclarative qt6.qtwayland qt6.qtsvg qt6.qtmultimedia
    wayland wayland-protocols wlroots_0_20 libxkbcommon libGL glib libinput
    pixman libdrm libxcb systemd
  ];
  cmakeFlags = [
    "-DBUILD_TESTING=OFF"
    "-DCMAKE_INSTALL_SYSCONFDIR=etc"
    "-DCMAKE_INSTALL_LIBEXECDIR=libexec"
  ];
  postPatch = ''
    printf '%s\n' '${revision}' > .lunadash-revision
  '';
  preFixup = ''
    qtWrapperArgs+=(--prefix PATH : "$out/bin:${runtimePath}")
    # Session scripts also need their dependencies before the compositor starts.
    wrapProgram "$out/bin/lunadash-update" --set LUNADASH_PACKAGE_MANAGER nix --prefix PATH : "${lib.makeBinPath [ bash coreutils gnused ]}"
    wrapProgram "$out/bin/ludash-session" --prefix PATH : "$out/bin:${runtimePath}"
    wrapProgram "$out/bin/lunadash-polkit-agent" --prefix PATH : "${runtimePath}"
    wrapProgram "$out/bin/lunadash-clipboard-history" --prefix PATH : "${lib.makeBinPath [ python3 wl-clipboard coreutils ]}"
  '';

  passthru.providedSessions = [ "lunadash" "ludash" ];
  meta = {
    description = "Development preview of the LunaDash Wayland desktop";
    homepage = "https://github.com/LuYishan-4/LunaDash";
    license = lib.licenses.gpl3Only;
    platforms = [ "x86_64-linux" "aarch64-linux" ];
    mainProgram = "lunadash-session";
  };
}
