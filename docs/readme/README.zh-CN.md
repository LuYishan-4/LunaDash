<div align="center">

<a href="https://luyishan-4.github.io/LunaDash/"><img src="../brand/banner.svg" alt="LunaDash" width="880"></a>

### 一个桌面环境。

整合日常所需，也让你按照习惯调整的 Wayland 桌面。<br>
**我们的目标：兼顾开箱即用与自定义，从第一次登录就开始好好使用。**

<p>
  <a href="#linux-distributions"><img src="https://img.shields.io/badge/status-development_preview-d3bfe6?style=flat-square" alt="Development preview"></a>
  <a href="https://github.com/LuYishan-4/LunaDash/actions/workflows/main-build.yml?query=branch%3Adev"><img src="https://github.com/LuYishan-4/LunaDash/actions/workflows/main-build.yml/badge.svg?branch=dev" alt="Build on dev"></a>
  <a href="https://github.com/LuYishan-4/LunaDash/pulls"><img src="https://img.shields.io/github/issues-pr/LuYishan-4/LunaDash?style=flat-square&amp;label=pull%20requests&amp;color=9ccbfb" alt="Open pull requests"></a>
  <a href="https://github.com/LuYishan-4/LunaDash/issues"><img src="https://img.shields.io/github/issues/LuYishan-4/LunaDash?style=flat-square&amp;color=d3bfe6" alt="Open issues"></a>
  <a href="../../LICENSE"><img src="https://img.shields.io/badge/license-GPL--3.0--only-9ccbfb?style=flat-square" alt="GPL-3.0-only license"></a>
</p>

<a id="linux-distributions"></a>

[![Arch Linux](https://img.shields.io/badge/Arch_Linux-1793d1?logo=arch-linux&logoColor=white&style=flat-square)](https://archlinux.org)
[![Fedora](https://img.shields.io/badge/Fedora-51A2DA?logo=fedora&logoColor=white&style=flat-square)](https://fedoraproject.org)
[![Ubuntu](https://img.shields.io/badge/Ubuntu_Rolling-E95420?logo=ubuntu&logoColor=white&style=flat-square)](https://ubuntu.com)

</div>

[English](../../README.md) · [繁體中文](README.zh-TW.md) · [简体中文](README.zh-CN.md) · [日本語](README.ja.md)

## LunaDash

- **日常功能一应俱全。** 面板、启动器、仪表盘、通知、设置与文件工具都包含在桌面中。
- **配合你的工作方式。** 可滚动的平铺窗口列，每列最多 8 个窗口；支持 Alt 拖动、十格工作区缩略图总览，任务栏可切换所有工作区的窗口。
- **把默认设置变成自己的风格。** 在设置中调整壁纸、颜色、间距、快捷键和默认应用程序。
- **需要时再深入。** 排列 shell 模块、修改 Quickshell/QML，或通过本地控制接口安排工作流程。

使用 **C++20 · C11 · wlroots · OpenGL · Qt 6 · Quickshell/QML** 开发。目前是持续开发中的预览版本，以 Arch Linux 为主要开发平台。

<a id="gallery"></a>

## 界面预览

<table>
  <tr>
    <td width="50%" align="center"><a href="../image/LunaDash-20260920-024312-792.png"><img src="../image/LunaDash-20260920-024312-792.png" alt="仪表盘整合时钟、系统状态与常用控制。" width="440"></a></td>
    <td width="50%" align="center"><a href="../image/LunaDash-20260920-024259-008.png"><img src="../image/LunaDash-20260920-024259-008.png" alt="在外观设置中选择壁纸与强调色。" width="440"></a></td>
  </tr>
  <tr>
    <td width="50%" align="center"><a href="../image/LunaDash-20260920-024708-948.png"><img src="../image/LunaDash-20260920-024708-948.png" alt="在可滚动的窗口列中使用终端、Discord 和 Zed。" width="440"></a></td>
    <td width="50%" align="center"><a href="../image/LunaDash-20260920-024455-959.png"><img src="../image/LunaDash-20260920-024455-959.png" alt="日历面板也能使用自定义图片。" width="440"></a></td>
  </tr>
</table>

<sub>实际桌面截图，拍摄于 2026 年 9 月 20 日。点击图片查看原图；布局与外观均可调整。</sub>

<a id="install"></a>

## 安装

获取开发分支后，运行会话安装程序：

```sh
git clone --branch dev https://github.com/LuYishan-4/LunaDash.git
cd LunaDash
./scripts/install-session.sh
```

安装程序会处理发行版依赖、编译 LunaDash 并安装登录会话。完成后注销，在登录管理器中选择 **LunaDash**。首次启动会显示欢迎消息与网站帮助链接。语言及桌面外观可在设置中调整。

可先运行 `./scripts/install-session.sh --dry-run` 预览安装步骤。桌面 shell 需要 **Quickshell 0.3+**；若发行版未提供，安装程序会指出缺少的依赖。选项与恢复方法请见[安装指南](../LOGIN_SESSION.md)，手动编译与嵌套会话请见[构建与测试](../TESTING_AND_FILES.md)。

## 常用快捷键

| 快捷键 | 功能 |
| --- | --- |
| `Super` + `Return` / `E` / `D` | 终端／文件／启动器 |
| `Super` + `T` | 打开 Kitty（默认终端） |
| `Super` + `H` / `L` | 聚焦左／右窗口列 |
| `Super` + `K` / `J` | 聚焦列内其他窗口 |
| `Super` + `F` | 最大化单个窗口／还原工作区内全部平铺窗口 |
| `Alt` + `Tab` | 十格工作区缩略图；松开 Alt 切换 |
| `Alt` + drag | 移动或交换位置；加 Shift 调整大小 |
| `Super` + `Shift` + `S` | 选取截图区域 |
| `Super` + `1`–`9` / `0` | 切换工作区 |

`Super` 即 Meta 键。可在 **设置 → 键盘快捷键** 中更改绑定。分组、调整大小与控制命令请见[设置文档](../SETTINGS.md)。

<a id="documentation"></a>

## 文档

| 开始使用 | 个性化 |
| --- | --- |
| [安装与运行](../LOGIN_SESSION.md) | [外观与配置](../CONFIGURATION.md) |
| [构建与测试](../TESTING_AND_FILES.md) | [设置与快捷键](../SETTINGS.md) |
| [显示器、DDC/CI 与启动](../DISPLAY_AND_STARTUP.md) | [Shell 模块](../MODULES.md) |
| [区域截图](../SCREEN_CAPTURE.md) | [默认应用程序与文件](../DEFAULT_APPS_AND_FILES.md) |
| [源码架构](../ARCHITECTURE.md) | [插件接口](../PLUGINS.md) |

<a id="contribute"></a>

## 参与贡献

一起让默认体验更好用，也让自定义更容易。欢迎报告问题、提出设计建议，以及贡献文档与代码。

PR 请以 **`dev`** 为目标分支。标题应清楚描述改动，说明用户可见的结果、实际运行的检查，以及影响的是默认行为还是可选的自定义功能。同步更新相关 **`docs/` 文档与网站内容**。

**PR 不得新增、修改、删除或重命名 `.github/workflows/` 下的文件、`site/src/pages/releases/` 下的发布 Markdown（`.md`／`.mdx`），或 `site/src/data/releases.json`。** 发布影响写在 PR 说明中，网站发布记录由 GitHub Release 自动生成。详见[贡献指南](../../CONTRIBUTING.md)与 [PR 模板](../../.github/pull_request_template.md)。

---

<p align="center"><img src="../brand/icon.svg" alt="" width="32"><br><strong>LunaDash</strong> · 项目目前尚不成熟，欢迎报告问题或提交 PR。<br><a href="../../LICENSE">GPL-3.0-only</a></p>
