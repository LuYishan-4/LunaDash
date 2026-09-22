# 視窗配置模板

[English](../en/WINDOW_LAYOUT_TEMPLATES.md) · [繁中索引](README.md)

`src/compositor/layout/WindowLayout.hpp` 是 compositor 與 layout implementation 之間的共用 contract；`LayoutTemplates.cpp` 註冊模式並建立 implementation。Compositor 只依賴 `WindowLayout`，不直接綁死 `TilingLayout`。

| Template | 狀態 | Implementation |
| --- | --- | --- |
| `tiling` | 預設可用 | `src/compositor/tiling/TilingLayout.cpp`；bounded split tiles/grouped rows |
| `stacking` | 透過 plugin | `src/compositor/layout/FreeformLayout.cpp`；persistent overlapping rectangles |

啟用有效的 native stacking replacement 時，host 會把目前 window inventory 與 focus 狀態 live migration 到 stacking；停用時回到 tiling。

## 新增 layout mode

1. 在小寫 source domain 中新增 interface/implementation，實作 `WindowLayout`，並明確加入 CMake。
2. 保持 window/workspace ID 在 insert/remove/minimize/workspace-move/focus 過程穩定。
3. `WindowPlacement` 只回 final geometry/visibility；layout 不應自己 resize client、建立 scene node、poll input 或跑 animation。
4. 正常 layout state 與 `presentation()` 分開；maximize 是 presentation，restore 應回到原 arrangement。
5. Factory 加 descriptor/implementation，strategy 切換時讓 compositor 轉移 window inventory/focus。
6. 用共用 interface 測 lifecycle、focus、bounds、workspace transfer、maximize/restore，再加 mode-specific overlap tests。

Compositor 的 shared scene animation 負責 opening/closing/layout/maximize/restore。Interactive resize 跟 pointer；drop 完成時 input controller 先解除 override，再 arrange，讓 moved window 和 neighbor 一起動畫到 final rectangle。

## SDK 2 strategy plugin

Stacking 已不是 placeholder；它由有效、enabled 的 `window-layout` replacement 且宣告 `windowTemplate: stacking` 時選用。兩種 strategy 都可以再通過 validated placement filter，host 仍掌握 membership 與 interaction state。實際 stacking / cascade policy 現在是 [`data/plugins/stacking-windows`](../../data/plugins/stacking-windows/) 下的正式 SDK 2 plugin package，而不是 SDK template。

詳見 [Plugin target contract](PLUGIN_TARGETS.md)。Enable/disable/rebuild hook 可在 callback 之間 live 套用，不需要重登。
