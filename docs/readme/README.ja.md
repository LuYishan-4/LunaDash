<div align="center">

<a href="https://luyishan-4.github.io/LunaDash/"><img src="../brand/banner.svg" alt="LunaDash" width="880"></a>

### デスクトップ環境。

日々の作業に必要な機能をまとめ、自分の使い方に合わせて調整できる Wayland デスクトップです。<br>
**目指すのは、初回ログインから使いやすく、自由にカスタマイズできる環境です。**

<p>
  <a href="#linux-distributions"><img src="https://img.shields.io/badge/status-development_preview-d3bfe6?style=flat-square" alt="Development preview"></a>
  <a href="https://github.com/LuYishan-4/LunaDash/actions/workflows/main-build.yml?query=branch%3Adev"><img src="https://github.com/LuYishan-4/LunaDash/actions/workflows/main-build.yml/badge.svg?branch=dev" alt="Build on dev"></a>
  <a href="https://github.com/LuYishan-4/LunaDash/pulls"><img src="https://img.shields.io/github/issues-pr/LuYishan-4/LunaDash?style=flat-square&amp;label=pull%20requests&amp;color=9ccbfb" alt="Open pull requests"></a>
  <a href="https://github.com/LuYishan-4/LunaDash/issues"><img src="https://img.shields.io/github/issues/LuYishan-4/LunaDash?style=flat-square&amp;color=d3bfe6" alt="Open issues"></a>
  <a href="../../LICENSE"><img src="https://img.shields.io/badge/license-GPL--3.0--only-9ccbfb?style=flat-square" alt="GPL-3.0-only license"></a>
</p>

[ウェブサイト](https://luyishan-4.github.io/LunaDash/) · [インストール](#install) · [ギャラリー](#gallery) · [ドキュメント](#documentation) · [貢献する](#contribute)

</div>

[English](../../README.md) · [繁體中文](README.zh-TW.md) · [简体中文](README.zh-CN.md) · [日本語](README.ja.md)

## すぐに使えて、自分に合わせられる

- **日常の機能をひとつに。** パネル、ランチャー、ダッシュボード、通知、設定、ファイル管理ツールを備えています。
- **作業に合ったウィンドウ管理。** スクロール可能なウィンドウ列、グループ化、フローティング表示、キーボード操作、ウィンドウアニメーションに対応しています。
- **好みに合う初期設定へ。** 壁紙、色、余白、ショートカット、既定のアプリを設定画面から変更できます。
- **必要に応じてさらに調整。** シェルモジュールの配置、Quickshell/QML の編集、ローカル制御インターフェースで使い方を広げられます。

**C++20 · C11 · wlroots · OpenGL · Qt 6 · Quickshell/QML** を使用しています。現在は開発中のプレビュー版で、主な開発環境は Arch Linux です。

<a id="gallery"></a>

## ギャラリー

<table>
  <tr>
    <td width="50%" align="center"><a href="../image/LunaDash-20260920-024312-792.png"><img src="../image/LunaDash-20260920-024312-792.png" alt="時計、システム状態、よく使う操作をダッシュボードに集約。" width="440"></a><sub>時計、システム状態、よく使う操作をダッシュボードに集約。</sub></td>
    <td width="50%" align="center"><a href="../image/LunaDash-20260920-024259-008.png"><img src="../image/LunaDash-20260920-024259-008.png" alt="外観設定で壁紙やアクセントカラーを変更。" width="440"></a><sub>外観設定で壁紙やアクセントカラーを変更。</sub></td>
  </tr>
  <tr>
    <td width="50%" align="center"><a href="../image/LunaDash-20260920-024708-948.png"><img src="../image/LunaDash-20260920-024708-948.png" alt="スクロール可能な列でターミナル、Discord、Zed を利用。" width="440"></a><sub>スクロール可能な列でターミナル、Discord、Zed を利用。</sub></td>
    <td width="50%" align="center"><a href="../image/LunaDash-20260920-024455-959.png"><img src="../image/LunaDash-20260920-024455-959.png" alt="カレンダーパネルの画像もカスタマイズ可能。" width="440"></a><sub>カレンダーパネルの画像もカスタマイズ可能。</sub></td>
  </tr>
</table>

<sub>2026 年 9 月 20 日に撮影した実際のデスクトップ画面です。画像をクリックすると原寸で表示します。配置と外観は変更できます。</sub>

<a id="install"></a>

## インストール

開発ブランチを取得し、セッションインストーラーを実行します。

```sh
git clone --branch dev https://github.com/LuYishan-4/LunaDash.git
cd LunaDash
./scripts/install-session.sh
```

インストーラーはディストリビューションの依存関係を処理し、LunaDash をビルドしてログインセッションをインストールします。完了したらログアウトし、ディスプレイマネージャーで **LunaDash** を選択してください。初回起動時には歓迎メッセージとヘルプサイトへのリンクが表示されます。言語とデスクトップの外観は設定画面で変更できます。

`./scripts/install-session.sh --dry-run` で実行内容を事前に確認できます。デスクトップシェルには **Quickshell 0.3+** が必要です。ディストリビューションで提供されていない場合は、不足する依存関係が表示されます。オプションと復旧方法は[インストールガイド](../LOGIN_SESSION.md)、手動ビルドやネストしたセッションは[ビルドとテスト](../TESTING_AND_FILES.md)を参照してください。

<a id="linux-distributions"></a>

## Linux ディストリビューション

| ディストリビューション | 現在の対応・検証範囲 |
| --- | --- |
| [![Arch Linux](https://img.shields.io/badge/Arch_Linux-1793D1?logo=archlinux&logoColor=white&style=flat-square)](https://archlinux.org) | 主な開発環境。ソースビルド CI、pacman/makepkg によるインストール |
| [![Ubuntu 24.04](https://img.shields.io/badge/Ubuntu_24.04-E95420?logo=ubuntu&logoColor=white&style=flat-square)](https://ubuntu.com) | 主要なビルド・実行時 CI。apt による依存関係の導入 |
| [![Debian 13](https://img.shields.io/badge/Debian_13-A81D33?logo=debian&logoColor=white&style=flat-square)](https://www.debian.org) | ソースビルド CI。apt による依存関係の導入 |
| [![Fedora 45](https://img.shields.io/badge/Fedora_45-51A2DA?logo=fedora&logoColor=white&style=flat-square)](https://fedoraproject.org) | ソースビルド CI。dnf による依存関係の導入 |
| [![openSUSE Tumbleweed](https://img.shields.io/badge/openSUSE_Tumbleweed-73BA25?logo=opensuse&logoColor=white&style=flat-square)](https://www.opensuse.org) | ソースビルド CI。zypper による依存関係の導入 |
| [![Alpine Edge](https://img.shields.io/badge/Alpine_Edge-0D597F?logo=alpinelinux&logoColor=white&style=flat-square)](https://alpinelinux.org) | ソースビルド CI。apk による依存関係の導入 |
| [![Void Linux](https://img.shields.io/badge/Void_Linux-478061?logo=voidlinux&logoColor=white&style=flat-square)](https://voidlinux.org) | インストーラー対応。現在のディストリビューション CI の対象外 |
| [![Gentoo](https://img.shields.io/badge/Gentoo-54487A?logo=gentoo&logoColor=white&style=flat-square)](https://www.gentoo.org) | インストーラー対応。現在のディストリビューション CI の対象外 |

その他のディストリビューションでも、依存関係を用意すれば標準の CMake インストール手順を利用できます。各コミットの結果は [Actions](https://github.com/LuYishan-4/LunaDash/actions?query=branch%3Adev) で確認してください。ビルドとソフトウェア描画のテストは、すべての GPU、モニター、独立したセッションの動作を保証するものではありません。画面ロックと画面共有／PipeWire portal はまだ未完成です。ネイティブプラグインは初期状態で無効で、有効にした場合はサンドボックス外で実行されます。

## 主なショートカット

| ショートカット | 操作 |
| --- | --- |
| `Super` + `Return` / `E` / `D` | ターミナル／ファイル／ランチャー |
| `Super` + `H` / `L` | 左／右の列にフォーカス |
| `Super` + `K` / `J` | 列内の別のウィンドウにフォーカス |
| `Super` + `Space` | フローティング表示を切り替え |
| `Super` + `Shift` + `S` | 範囲を選択してスクリーンショット |
| `Super` + `1`–`9` | ワークスペースを切り替え |

`Super` は Meta キーです。**設定 → キーボードショートカット** から変更できます。グループ化、サイズ変更、制御コマンドについては[設定ドキュメント](../SETTINGS.md)を参照してください。

<a id="documentation"></a>

## ドキュメント

| はじめに | カスタマイズ |
| --- | --- |
| [インストールと実行](../LOGIN_SESSION.md) | [外観と構成](../CONFIGURATION.md) |
| [ビルドとテスト](../TESTING_AND_FILES.md) | [設定とショートカット](../SETTINGS.md) |
| [ディスプレイ・DDC/CI・起動](../DISPLAY_AND_STARTUP.md) | [シェルモジュール](../MODULES.md) |
| [範囲指定スクリーンショット](../SCREEN_CAPTURE.md) | [既定のアプリとファイル](../DEFAULT_APPS_AND_FILES.md) |
| [ソース構成](../ARCHITECTURE.md) | [プラグインインターフェース](../PLUGINS.md) |

<a id="contribute"></a>

## 貢献する

初期状態の使いやすさと、カスタマイズのしやすさを一緒に改善しましょう。不具合報告、デザインへの意見、ドキュメントやコードの貢献を歓迎します。

PR の対象ブランチは **`dev`** です。変更内容が伝わるタイトルを付け、利用者から見た結果、実際に行った検証、既定の動作と任意のカスタマイズのどちらに影響するかを説明してください。関連する **`docs/` とウェブサイトの内容** も更新してください。

**PR では `.github/workflows/` 内のファイル、`site/src/pages/releases/` 内のリリース Markdown（`.md`／`.mdx`）、`site/src/data/releases.json` の追加・変更・削除・名前変更は禁止です。** リリースへの影響は PR 本文に記載してください。サイトのリリースノートは GitHub Release から自動生成されます。[貢献ガイド](../../CONTRIBUTING.md)と [PR テンプレート](../../.github/pull_request_template.md)も参照してください。

---

<p align="center"><img src="../brand/icon.svg" alt="" width="32"><br><strong>LunaDash</strong> · すぐに使えて、自分らしく。<br><a href="../../LICENSE">GPL-3.0-only</a></p>
