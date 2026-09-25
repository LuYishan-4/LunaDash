# 視窗配置模板

[English](../en/WINDOW_LAYOUT_TEMPLATES.md) · [繁中索引](README.md)

`src/compositor/layout/WindowLayout.hpp` 是 compositor 與 layout implementation 之間的共用 contract；`src/compositor/window/WindowTemplate.cpp` 註冊模式、設定及建立 implementation 的 factory。Compositor 只依賴 `WindowLayout`，不直接綁死 `TilingLayout`。

| Template | 狀態 | Implementation |
| --- | --- | --- |
| `tiling` | 預設可用 | `src/compositor/tiling/TilingLayout.cpp`；bounded split tiles/grouped rows |
| `stacking` | 透過 plugin | `src/compositor/layout/FreeformLayout.cpp`；persistent overlapping rectangles |

啟用有效的 native stacking replacement 時，host 會把目前 window inventory 與 focus 狀態 live migration 到 stacking；停用時回到 tiling。

## 焦點分割配置

預設排列沿用現有 bounded split tree。前兩個視窗左右分割，第一個視窗保留在右側；之後的新視窗分割目前焦點所在的格子，各層交替使用上下及左右分割。開啟程式前先聚焦某個格子，即可指定分割區域；其餘分支保持原有幾何配置。焦點分割會讓新格子低於建議最小尺寸（預設 320 × 220 邏輯像素）時，改為分割可容納新窗格的最大格子，必要時嘗試另一分割方向。若所有格子皆無法滿足建議尺寸，仍分割最大的格子，維持所有視窗在畫面內且不重疊。這是配置偏好，不保證滿足各應用程式的最小尺寸。群組視窗仍排列在原有 leaf 裡，群組、共享邊界縮放、最小化／還原及最大化／還原繼續使用同一模板。

要完全重現六視窗參考圖的密集排列，先降低兩個建議最小尺寸（可低至 1），再開啟三個視窗，再聚焦左上方視窗，接著開啟三個視窗。第一個視窗維持右半部完整高度，左下格子保持原狀，左上分支則逐層細分。預設建議尺寸則允許右側大窗先行分割，減少左側控制項過度擠壓。單純變更焦點不會重新排列格子。

設定 > 視窗與工作區 > 視窗配置提供：

| 設定 | 預設 | 行為 |
| --- | --- | --- |
| `splitTarget` | `focused` | 以交替方向分割目前焦點格子；`largest` 則選取最大格子並沿較長的尺寸分割。 |
| `firstWindowSide` | `right` | 首次左右分割時，原有視窗保留在右側；選擇 `left` 則放在左側。 |
| `gap` | 繼承桌面間距，初始為 12 | 相鄰格子的間距。 |
| `minimumTileWidth` | 320 | 新格子的建議最小寬度，畫面擁擠時可縮小。 |
| `minimumTileHeight` | 220 | 新格子的建議最小高度，畫面擁擠時可縮小。 |
| `defaultWidth` | 1120 | 單一視窗未提供偏好尺寸時採用的初始寬度。 |

首對視窗固定使用左右排列。分割策略與首個視窗方向的修改會套用到之後的新視窗、移入工作區及移出群組操作，既有排列保持不變；已明確儲存的設定優先。這些欄位由原有 `layout:tiling` 設定目標驗證並儲存在 `windowLayout/tiling/` 設定鍵下，不新增設定檔、Niri 解析器或配置 backend。選擇 `largest` 及 `left` 可在一般橫向螢幕恢復先前的插入策略。

原有 layout CI target 已補上參考排列、策略切換、目的工作區焦點、最小化恢復、小尺寸邊界及最大化／還原的 regression。依使用者要求，本機未建置或執行測試；CI 結果及真實工作階段視覺驗證須分別記錄。

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
