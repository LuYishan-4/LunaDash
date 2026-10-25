{ lib, stdenv, runCommand, cmake, ninja, pkg-config, python3, makeWrapper
, rustc, cargo, rustPlatform, corrosion
, nlohmann_json
, qt6, wayland, wayland-protocols, wayland-scanner, wlroots_0_20
, libxkbcommon, libGL, libglvnd, vulkan-headers, vulkan-loader, glib, libinput, pixman, libdrm, libxcb, systemd, dbus, pam
, coreutils, bash, gnused, curl, gdk-pixbuf, xwayland, xdg-utils, wl-clipboard, grim, slurp
, brightnessctl, ddcutil, wireplumber, pulseaudio, kdePackages, fontconfig, freetype
, xdg-desktop-portal-wlr, revision ? "unknown", shellSource, slintSource, slintluSource
}:
let
  slintCargoDeps = rustPlatform.importCargoLock {
    lockFile = "${slintSource}/Cargo.lock";
  };
  # Slint's C++ API with the features CMakeLists.txt requires. As its own derivation it is
  # built once per Slint revision and then comes from the Nix store (and the CI cache).
  slintCpp = stdenv.mkDerivation {
    pname = "slint-cpp";
    version = "1.18.1";
    src = slintSource;
    cmakeDir = "../api/cpp";
    nativeBuildInputs = [ cmake ninja pkg-config rustc cargo rustPlatform.cargoSetupHook corrosion ];
    cargoDeps = slintCargoDeps;
    buildInputs = [ wayland libxkbcommon fontconfig freetype libGL ];
    cmakeFlags = [
      "-DSLINT_FEATURE_BACKEND_QT=OFF"
      "-DSLINT_FEATURE_BACKEND_WINIT=OFF"
      "-DSLINT_FEATURE_BACKEND_WINIT_WAYLAND=ON"
      "-DSLINT_FEATURE_BACKEND_WINIT_X11=OFF"
      "-DSLINT_FEATURE_RENDERER_FEMTOVG=ON"
      "-DSLINT_FEATURE_RENDERER_SOFTWARE=OFF"
      "-DSLINT_FEATURE_INTERPRETER=OFF"
      "-DSLINT_FEATURE_SYSTEM_TRAY=OFF"
      "-DSLINT_FEATURE_TESTING=OFF"
    ];
  };
  # SlintLu, the shell runtime, at the revision in packaging/slintlu-revision. EGL and Vulkan
  # are loaded at runtime, so their loaders go on the library path.
  slintlu = rustPlatform.buildRustPackage {
    pname = "slintlu";
    version = "unstable";
    src = slintluSource;
    cargoLock.lockFile = "${slintluSource}/Cargo.lock";
    nativeBuildInputs = [ pkg-config makeWrapper ];
    buildInputs = [ wayland libxkbcommon fontconfig freetype pam ];
    # SlintLu's own CI runs its tests; the session only needs the binary.
    doCheck = false;
    postFixup = ''
      wrapProgram $out/bin/slintlu \
        --prefix LD_LIBRARY_PATH : ${lib.makeLibraryPath [ libGL vulkan-loader wayland libxkbcommon ]}
    '';
  };
  runtimePath = lib.makeBinPath [
    coreutils bash gnused dbus systemd xwayland xdg-utils wl-clipboard
    grim slurp brightnessctl ddcutil wireplumber pulseaudio
  ] + ":${xdg-desktop-portal-wlr}/libexec:${kdePackages.polkit-kde-agent-1}/libexec";
in
stdenv.mkDerivation {
  pname = "lunadash";
  version = "1.0.2a";
  src = lib.fileset.toSource {
    root = ../.;
    fileset = lib.fileset.unions [
      ../CMakeLists.txt ../cmake ../src ../data
      ../protocols ../scripts ../tests ../LICENSE
      ../docs/PLUGIN_TARGETS.md ../docs/PLUGINS.md
    ];
  };

  nativeBuildInputs = [ cmake ninja pkg-config python3 makeWrapper wayland-scanner
    qt6.wrapQtAppsHook ];
  buildInputs = [ slintCpp
    qt6.qtbase qt6.qtdeclarative qt6.qtwayland qt6.qtsvg qt6.qtmultimedia fontconfig freetype
    vulkan-headers vulkan-loader wayland wayland-protocols wlroots_0_20 libxkbcommon libGL glib libinput
    pixman libdrm libxcb systemd nlohmann_json libglvnd gdk-pixbuf curl
  ];
  cmakeFlags = [
    "-DBUILD_TESTING=OFF"
    "-DCMAKE_INSTALL_SYSCONFDIR=etc"
    "-DCMAKE_INSTALL_LIBEXECDIR=libexec"
    "-DLUDASH_SHELL_SOURCE_DIR=${shellSource}"
    "-DLUDASH_SYSTEM_SLINT=ON"
  ];
  postPatch = ''
    printf '%s\n' '${revision}' > .lunadash-revision
  '';
  postInstall = ''
    install -d "$out/libexec/lunadash"
    ln -s ${slintlu}/bin/slintlu "$out/libexec/lunadash/slintlu"
  '';
  preFixup = ''
    qtWrapperArgs+=(--prefix PATH : "$out/bin:${runtimePath}")
    # Session scripts also need their dependencies before the compositor starts.
    wrapProgram "$out/bin/lunadash-update" --set LUNADASH_PACKAGE_MANAGER nix --prefix PATH : "${lib.makeBinPath [ bash coreutils gnused ]}"
    wrapProgram "$out/bin/ludash-session" --prefix PATH : "$out/bin:${runtimePath}"
    wrapProgram "$out/bin/lunadash-polkit-agent" --prefix PATH : "${runtimePath}"
    wrapProgram "$out/bin/lunadash-clipboard-history" --prefix PATH : "${lib.makeBinPath [ python3 wl-clipboard coreutils ]}"
  '';

  passthru = {
    providedSessions = [ "lunadash" "ludash" ];
    inherit slintlu slintCpp;
  };
  meta = {
    description = "Development preview of the LunaDash Wayland desktop";
    homepage = "https://github.com/LuYishan-4/LunaDash";
    license = lib.licenses.gpl3Only;
    platforms = [ "x86_64-linux" "aarch64-linux" ];
    mainProgram = "lunadash-session";
  };
}
