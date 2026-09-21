# 共用設定 API 與自動設定介面

Compositor 現在透過同一個 API 描述原生 effect、Quickshell/OpenGL 插件、
內建功能、視窗配置與 Shell 模組的設定。Effect 不必自行寫設定頁；只要在
SDK manifest 的 `settings` 宣告選項，就會自動出現在共用設定介面。
即使插件尚未啟用也可以調整選項，儲存選項不會偷偷啟用插件或信任程式碼。

## 讀取與修改

`lunadashctl settings-describe` 回傳 `{version: 1, targets: [...]}`，
同一份資料也放在 `status.settingsApi`，沿用現有 Shell 狀態通道。
每個 target 包含 `id`、`name`、`type`、`category`、`schema`、`values`、`revision`。

Target 命名：`plugin:<插件 ID>`、`builtin:<功能 ID>`、
`layout:<目前模板>`、`module:<模組 ID>:<設定區段>`。
模組區段為 `module`、`style`、`config`、`custom`。
視窗配置使用模板提供的選項，不再顯示另一個會互相衝突的舊 gap 編輯器。

```sh
lunadashctl settings-describe
# revision 必須使用剛讀取到的該 target 版本。
lunadashctl settings-update '{"target":"plugin:org.ludash.fade","revision":"<目前版本>","changes":{"duration":300,"softFocus":false}}'
```

更新時會先檢查 target、schema、型別、範圍、唯讀欄位與版本，再只合併這個
設定目標的選項到現有文件。未知欄位或過期版本會回傳錯誤；成功則回傳更新後
的 Shell 狀態與 `settingsTarget`。檔案仍由既有原子寫入程序儲存。
版本檢查用來避免舊表單蓋掉新設定，不等同任意外部程式之間的跨程序鎖。

表單提供套用、重新載入、預設值。修改先留在草稿，套用才寫入；錯誤不會
丟棄草稿。若後端設定已改變，必須重新載入。插件啟用與組合模式仍須明確操作。

## 四種標準控制項

| `control` | 資料 | 現有 QML 元件 |
| --- | --- | --- |
| `toggle` | boolean | `SoftSwitch`，是／否 |
| `select` | `enum` 中的值 | `StyledComboBox`；翻譯文字不改動儲存值 |
| `number` | integer / number | `SoftField` 與減少／增加按鈕 |
| `slider` | 有範圍的 integer / number | `SettingsSlider` / `SoftSlider` |

可宣告 `label`、`description`、`order`、`minimum`、`maximum`、`step`、
`enum`、`readOnly`。省略 `control` 時，列舉自動用下拉、布林用開關、
有上下限的數值用滑條，其餘數值用輸入框。也可明確選擇 `number`，
不要所有數字都變成滑條。`step` 是按鈕／滑條的增量，不限制手動輸入必須落在
固定格點；小數不會被強制變成整數。SDK 與後端皆拒絕非有限數字、
JavaScript 無法安全表示的整數、錯誤型別、重複列舉與不相容控制項。

```json
{
  "softFocus": {"type":"boolean","default":true,"control":"toggle"},
  "easing": {"type":"string","default":"linear","enum":["linear","outCubic"],"control":"select"},
  "duration": {"type":"integer","default":260,"minimum":0,"maximum":600,"step":10,"control":"number"},
  "exitScale": {"type":"number","default":0.9,"minimum":0.5,"maximum":1,"step":0.01,"control":"slider"}
}
```

既有的路徑、色碼與自由文字保留 `text` 相容編輯器，不能硬塞成數字或下拉。
既有的有限字串集合則使用多選開關。這些欄位仍受後端路徑／pattern 驗證限制。
`specialValues` 可表達「0 代表自動尺寸」等舊值，使用數值輸入而非跨越非法
區間的滑條。`data/plugins/fade` 示範四種標準控制項，每個選項都有進入原生
hook 的實際用途，不只是顯示用 metadata。

SDK ABI 2 與 manifest schemaVersion 2 不變；manifest 改動仍需重建插件。
`core/settings/SettingsSchema.hpp` 與安裝版 SDK `SettingsSchema.py` 共用
`tests/settings/contract.json` 測試資料，防止建置與執行時驗證規則不一致。

## 模組化設定

Shell 模組設定頁可以按實作類型、分類、搜尋文字，以及是否有可調選項篩選。
`effect`、`quickshell`、`opengl`、`module`、`layout`、`builtin` 共用表單，
沿用現有 Theme、字體、按鈕與 QML 控制項。

`data/modules/registry.json` 集中管理模組 ID、共用區段 schema、各模組覆寫、
預設值、範圍、標籤與模板是否可用。後端先合併共用區段再提供描述；QML 不再
維護另一份模組名稱、數值限制或大量手寫設定欄位。舊模組 JSON 格式仍可讀取。
Settings、setup、feedback 的恢復入口不能停用；面板的跨欄位限制、自訂 QML
正規路徑、檔案大小、符號連結檢查與程式碼信任仍由主機端控制。

## 視窗操作邊界

配置專屬操作使用 `performAction`，不再為每個方向、分組方法增加共用介面的
pure virtual。模板操作描述提供參數 schema 與必填欄位，缺參數、錯誤 ID、
小數 ID、非法方向與未知欄位都在呼叫前拒絕。`window-layout-action` 的工作
區域由 compositor 提供，呼叫者不能自行傳入 `area`。協議與繪圖資源生命週期
不會暴露成任意可調設定或插件 callback。
