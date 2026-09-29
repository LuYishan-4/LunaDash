# NixOS

[English](../en/NIXOS.md) · [繁中索引](README.md)

LunaDash **1.0.1a** 提供 Nix flake、套件與 NixOS 模組。Arch Linux 仍是主要開發平台。目前是開發預覽版，請保留另一個可用的桌面工作階段。

## 加入登入工作階段

在已啟用 flakes 的 NixOS 設定中加入：

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

執行 `sudo nixos-rebuild switch --flake .#my-host`，登出後在登入畫面選擇 **LunaDash (Wayland)**。模組會註冊工作階段，但不會替主機選擇或啟用顯示管理器。使用 SDDM 的主機可自行設定 `services.displayManager.sddm.enable = true`。TTY 登入經 PAM/logind 建立使用者 runtime 目錄後，也能執行 `lunadash-session`。

套件提供 `x86_64-linux` 與 `aarch64-linux`；CI 建置並執行 x86_64，aarch64 只驗證求值。使用 `follows` 會採用主機的 Qt 與 wlroots 套件，專案提交的 lock 才是 CI 使用的基準。較舊的穩定版 nixpkgs 尚未涵蓋。套件使用 wlroots 0.19、Quickshell 0.3+，包含 Qt 多媒體與 SVG 模組。

## 原始碼布局

根目錄保留 `flake.nix` 與 `flake.lock`。Nix 實作集中於最外層 `nix/`：`package.nix` 管理套件與 runtime wrapper，`module.nix` 管理 NixOS 選項及服務，`checks.nix` 管理套件、模組與 runtime 驗證。

## 模組設定範圍

- 安裝 LunaDash，註冊登入項目，啟用 graphics、polkit、DConf 及選用的 XWayland 相容層。
- 安裝預設終端 Kitty 與檔案管理器 Dolphin。設為 `programs.lunadash.enableDefaultApps = false` 可自行選擇應用程式。
- 設定 LunaDash FileChooser／Settings portal、wlr ScreenCast／Screenshot 與 GTK fallback，並註冊 D-Bus 與 systemd 使用者服務。
- 安裝 `/etc/xdg/xdg-desktop-portal-wlr/LunaDash` 的螢幕分享選擇器設定。

網路、藍牙、PipeWire、WirePlumber、輸入法、字型與顯示管理器由主機設定決定。音效與螢幕分享需要 PipeWire；請啟用 `services.pipewire.enable = true`，再依主機需要設定其他服務。Fcitx 請使用 NixOS 的 `i18n.inputMethod` 選項並安裝需要的語言引擎。只安裝套件不會啟用這些系統服務。

執行檔、QML、素材與 portal metadata 都保留在 Nix store，wrapper 提供程式與 Qt 模組搜尋路徑。原生外掛預設關閉；metadata 驗證不提供隔離。

## 建置、開發與更新

```sh
nix build .#lunadash
nix develop
nix flake check --print-build-logs --no-update-lock-file
```

以上是開發指令，不會啟用系統登入工作階段。Flake 另提供 `overlays.default` 與 `nixosModules.lunadash`，可透過 `programs.lunadash.package` 選擇覆寫後的套件。

更新主機 flake 的 `lunadash` input，再重新建置系統：

```sh
nix flake update lunadash
sudo nixos-rebuild switch --flake .#my-host
# 需要時還原前一個系統世代：
sudo nixos-rebuild switch --rollback
```

NixOS 的安裝、更新與回復都使用系統世代。`install.sh`、`install-session.sh` 與桌面的原始碼更新器不適用於 NixOS。Session／dependency installer 會拒絕 NixOS；套件內的更新器會顯示 NixOS 更新方式，不要求提權，也不寫入系統檔案。

## 驗證範圍

Nix CI 會求值模組斷言及 session／portal 註冊、建置安裝後的套件，再透過已安裝 wrapper 啟動 headless wlroots／pixman compositor。狀態 JSON 保留為 CI artifact。這不代表已驗證 NixOS 的實際登入畫面、Quickshell 外觀、實體輸入、GPU 驅動、多螢幕或 PipeWire 擷取；發行前仍需要真實工作階段及截圖。詳見 [CI](CI.md) 與[測試指南](TESTING_AND_FILES.md)。

套件實作依循上游 [Qt wrapper 說明](https://github.com/NixOS/nixpkgs/blob/master/doc/languages-frameworks/qt.section.md)與 [NixOS portal 模組](https://github.com/NixOS/nixpkgs/blob/master/nixos/modules/config/xdg/portal.nix)。

`scripts/make-source.sh` 產生的原始碼封存檔也包含 flake、lock 與 `nix/` 定義。
