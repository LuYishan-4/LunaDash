#include "desktop/shortcuts/KeyBindings.hpp"

#include "config/desktop/DesktopPreferenceStore.hpp"
#include "core/log/Log.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <tuple>
#include <utility>

namespace LunaDash {
namespace {

nlohmann::json defaults() {
  nlohmann::json normal{
      {"Meta+H", "focus-column-left"},
      {"Meta+L", "focus-column-right"},
      {"Meta+K", "focus-window-up"},
      {"Meta+J", "focus-window-down"},
      {"Meta+Ctrl+H", "move-column-left"},
      {"Meta+Ctrl+L", "move-column-right"},
      {"Meta+Left", "tile-window left"},
      {"Meta+Right", "tile-window right"},
      {"Meta+Up", "tile-window up"},
      {"Meta+Down", "tile-window down"},
      {"Meta+S", "toggle-window-style"},
      {"Meta+bracketleft", "consume-or-expel-window-left"},
      {"Meta+bracketright", "consume-or-expel-window-right"},
      {"Meta+period", "expel-window-from-column"},
      {"Meta+C", "center-column"},
      {"Meta+equal", "set-column-width +10%"},
      {"Meta+minus", "set-column-width -10%"},
      {"Meta+F", "maximize-column"},
      {"Meta+Shift+F", "fullscreen-window"},
      {"F12", "fullscreen-window"},
      {"Meta+Q", "close-window"},
      {"Meta+M", "minimize-window"},
      {"Meta+T", "spawn-default terminal"},
      {"Meta+E", "spawn-default files"},
      {"Meta+D", "toggle-launcher"},
      {"Meta+A", "open-orbit"},
      {"Meta+I", "open-control-center"},
      {"Meta+V", "open-clipboard"},
      {"Meta+X", "open-power-menu"},
      {"Meta+Shift+S", "screenshot"},
      {"Meta+W", "choose-wallpaper"},
      {"Meta+Ctrl+W", "random-wallpaper"},
      {"Meta+N", "toggle-night-light"},
      {"Meta+Tab", "toggle-overview"}};
  for (int workspace = 1; workspace <= 10; ++workspace) {
    const auto key = std::to_string(workspace % 10);
    normal["Meta+" + key] = "focus-workspace " + std::to_string(workspace);
    normal["Meta+Ctrl+" + key] =
        "move-window-to-workspace " + std::to_string(workspace);
  }
  nlohmann::json overview{
      {"Escape", "close-overview"},
      {"Return", "close-overview"},
      {"KP_Enter", "close-overview"},
      {"Left", "focus-column-left"},
      {"H", "focus-column-left"},
      {"Right", "focus-column-right"},
      {"L", "focus-column-right"},
      {"Up", "focus-workspace-up"},
      {"K", "focus-workspace-up"},
      {"Down", "focus-workspace-down"},
      {"J", "focus-workspace-down"},
      {"Ctrl+Left", "move-column-left"},
      {"Ctrl+H", "move-column-left"},
      {"Ctrl+Right", "move-column-right"},
      {"Ctrl+L", "move-column-right"},
      {"Ctrl+Up", "move-window-to-workspace-up"},
      {"Ctrl+K", "move-window-to-workspace-up"},
      {"Ctrl+Down", "move-window-to-workspace-down"},
      {"Ctrl+J", "move-window-to-workspace-down"}};
  return {{"normal", std::move(normal)}, {"overview", std::move(overview)}};
}

// A former binding name ({"closeWindow": "Meta+Q"}): the mode and action it
// became, and the key it had by default.
struct Former {
  std::string mode;
  std::string action;
  std::string defaultKeys;
};

std::optional<Former> former(std::string_view name) {
  struct Row {
    std::string_view name, mode, action, keys;
  };
  static const Row rows[] = {
      {"focusLeft", "normal", "focus-column-left", "Meta+H"},
      {"focusRight", "normal", "focus-column-right", "Meta+L"},
      {"focusUp", "normal", "focus-window-up", "Meta+K"},
      {"focusDown", "normal", "focus-window-down", "Meta+J"},
      {"reorderLeft", "normal", "move-column-left", "Meta+Ctrl+H"},
      {"reorderRight", "normal", "move-column-right", "Meta+Ctrl+L"},
      {"groupLeft", "normal", "consume-or-expel-window-left", "Meta+bracketleft"},
      {"groupRight", "normal", "consume-or-expel-window-right", "Meta+bracketright"},
      {"expelWindow", "normal", "expel-window-from-column", "Meta+period"},
      {"centerColumn", "normal", "center-column", "Meta+C"},
      {"widenColumn", "normal", "set-column-width +10%", "Meta+equal"},
      {"narrowColumn", "normal", "set-column-width -10%", "Meta+minus"},
      {"maximizeWindow", "normal", "maximize-column", "Meta+F"},
      {"toggleFullscreen", "normal", "fullscreen-window", "Meta+Shift+F"},
      {"closeWindow", "normal", "close-window", "Meta+Q"},
      {"minimizeWindow", "normal", "minimize-window", "Meta+M"},
      {"launchTerminal", "normal", "spawn-default terminal", "Meta+T"},
      {"launchFiles", "normal", "spawn-default files", "Meta+E"},
      {"launchLauncher", "normal", "toggle-launcher", "Meta+D"},
      {"launchOrbit", "normal", "open-orbit", "Meta+A"},
      {"openControlCenter", "normal", "open-control-center", "Meta+I"},
      {"openClipboard", "normal", "open-clipboard", "Meta+V"},
      {"openPowerMenu", "normal", "open-power-menu", "Meta+X"},
      {"screenshot", "normal", "screenshot", "Meta+Shift+S"},
      {"chooseWallpaper", "normal", "choose-wallpaper", "Meta+W"},
      {"randomWallpaper", "normal", "random-wallpaper", "Meta+Ctrl+W"},
      {"toggleEyeCare", "normal", "toggle-night-light", "Meta+N"},
      {"toggleOverview", "normal", "toggle-overview", "Meta+Tab"},
      {"overviewClose", "overview", "close-overview", "Escape"},
      {"overviewAccept", "overview", "close-overview", "Return"},
      {"overviewAcceptKeypad", "overview", "close-overview", "KP_Enter"},
      {"overviewFocusLeft", "overview", "focus-column-left", "Left"},
      {"overviewFocusLeftVim", "overview", "focus-column-left", "H"},
      {"overviewFocusRight", "overview", "focus-column-right", "Right"},
      {"overviewFocusRightVim", "overview", "focus-column-right", "L"},
      {"overviewWorkspacePrevious", "overview", "focus-workspace-up", "Up"},
      {"overviewWorkspacePreviousVim", "overview", "focus-workspace-up", "K"},
      {"overviewWorkspaceNext", "overview", "focus-workspace-down", "Down"},
      {"overviewWorkspaceNextVim", "overview", "focus-workspace-down", "J"},
      {"overviewReorderLeft", "overview", "move-column-left", "Ctrl+Left"},
      {"overviewReorderLeftVim", "overview", "move-column-left", "Ctrl+H"},
      {"overviewReorderRight", "overview", "move-column-right", "Ctrl+Right"},
      {"overviewReorderRightVim", "overview", "move-column-right", "Ctrl+L"},
      {"overviewMoveToPreviousWorkspace", "overview", "move-window-to-workspace-up", "Ctrl+Up"},
      {"overviewMoveToPreviousWorkspaceVim", "overview", "move-window-to-workspace-up", "Ctrl+K"},
      {"overviewMoveToNextWorkspace", "overview", "move-window-to-workspace-down", "Ctrl+Down"},
      {"overviewMoveToNextWorkspaceVim", "overview", "move-window-to-workspace-down", "Ctrl+J"}};
  for (const auto &row : rows)
    if (row.name == name)
      return Former{std::string(row.mode), std::string(row.action), std::string(row.keys)};
  // workspace3 and moveToWorkspace3: Meta+3 and Meta+Ctrl+3 (10 on 0).
  for (const auto &[prefix, action, keys] :
       {std::tuple{std::string_view("moveToWorkspace"),
                   std::string_view("move-window-to-workspace "), std::string_view("Meta+Ctrl+")},
        std::tuple{std::string_view("workspace"), std::string_view("focus-workspace "),
                   std::string_view("Meta+")}})
    if (name.starts_with(prefix)) {
      int number = 0;
      const auto suffix = name.substr(prefix.size());
      const auto parsed = std::from_chars(suffix.data(), suffix.data() + suffix.size(), number);
      if (parsed.ec != std::errc{} || parsed.ptr != suffix.data() + suffix.size() || number < 1)
        return std::nullopt;
      return Former{"normal", std::string(action) + std::to_string(number),
                    std::string(keys) + std::to_string(number % 10)};
    }
  return std::nullopt;
}

// The former bindings over the defaults: each stored action loses its
// default key and gets the stored one ("Disabled" leaves it unbound).
nlohmann::json migrate(const nlohmann::json &shortcuts) {
  auto result = defaults();
  for (auto it = shortcuts.begin(); it != shortcuts.end(); ++it) {
    const auto previous = former(it.key());
    if (!previous || !it.value().is_string())
      continue;
    auto &bindings = result[previous->mode];
    if (const auto keys = parseKeyCombo(previous->defaultKeys);
        keys && bindings.value(keys->canonical, "") == previous->action)
      bindings.erase(keys->canonical);
    if (const auto keys = parseKeyCombo(it.value().get<std::string>()))
      bindings[keys->canonical] = previous->action;
  }
  return result;
}

xkb_keysym_t keySymbol(std::string_view name) {
  if (name == "=")
    return XKB_KEY_equal;
  if (name == "-")
    return XKB_KEY_minus;
  const std::string key(name);
  return xkb_keysym_from_name(key.c_str(), XKB_KEYSYM_CASE_INSENSITIVE);
}

// Alt+Tab drives the window switcher, which the compositor handles itself.
bool reservedForSwitcher(const KeyCombo &combo) {
  return (combo.symbol == XKB_KEY_Tab || combo.symbol == XKB_KEY_ISO_Left_Tab) &&
         (combo.modifiers == KeyAlt || combo.modifiers == (KeyAlt | KeyShift));
}

bool knownMode(std::string_view mode) {
  const auto &modes = KeyBindings::modes();
  return std::find(modes.begin(), modes.end(), mode) != modes.end();
}

void setError(std::string *error, std::string message) {
  if (error)
    *error = std::move(message);
}

} // namespace

std::optional<KeyCombo> parseKeyCombo(std::string_view text) {
  uint32_t modifiers = 0;
  std::string_view key;
  std::size_t start = 0;
  while (start <= text.size()) {
    const auto separator = text.find('+', start);
    // "Meta++" (a plus key) ends with an empty piece after the separator.
    const auto piece = text.substr(
        start, separator == std::string_view::npos ? text.size() - start
                                                   : separator - start);
    if (piece == "Meta" || piece == "Super" || piece == "Mod")
      modifiers |= KeyMeta;
    else if (piece == "Ctrl" || piece == "Control")
      modifiers |= KeyControl;
    else if (piece == "Alt")
      modifiers |= KeyAlt;
    else if (piece == "Shift")
      modifiers |= KeyShift;
    else if (key.empty() && !piece.empty())
      key = piece;
    else
      return std::nullopt;
    if (separator == std::string_view::npos)
      break;
    start = separator + 1;
  }
  if (key.empty())
    return std::nullopt;
  const xkb_keysym_t symbol = keySymbol(key);
  if (symbol == XKB_KEY_NoSymbol)
    return std::nullopt;

  // The keysym's own name, so "minus", "-" and "Minus" are one key; letters
  // are written in upper case.
  char name[64] = {};
  xkb_keysym_get_name(xkb_keysym_to_lower(symbol), name, sizeof name);
  std::string canonicalKey = name;
  if (canonicalKey.size() == 1)
    canonicalKey.front() = static_cast<char>(
        std::toupper(static_cast<unsigned char>(canonicalKey.front())));
  std::string canonical;
  for (const auto &[bit, label] :
       {std::pair{KeyMeta, "Meta"}, std::pair{KeyControl, "Ctrl"},
        std::pair{KeyAlt, "Alt"}, std::pair{KeyShift, "Shift"}})
    if (modifiers & bit)
      canonical += std::string(label) + "+";
  canonical += canonicalKey;
  return KeyCombo{symbol, modifiers, std::move(canonical)};
}

const std::vector<std::string> &KeyBindings::modes() {
  static const std::vector<std::string> names{"normal", "overview"};
  return names;
}

KeyBindings::KeyBindings(Validator validate) : validate_(std::move(validate)) {
  auto stored = loadKeyBindings();
  if (stored.empty()) {
    const auto former = loadActionShortcuts();
    if (!former.empty()) {
      std::string error;
      if (!store(migrate(former), &error))
        logWarning("Key bindings were not migrated: %s", error.c_str());
      return;
    }
    bindings_ = defaults();
    index();
    return;
  }
  // Written by hand too: keys are made canonical, and what does not parse or
  // names no action is left out (and logged).
  bindings_ = nlohmann::json::object();
  for (const auto &mode : modes()) {
    bindings_[mode] = nlohmann::json::object();
    const auto found = stored.find(mode);
    if (found == stored.end() || !found->is_object())
      continue;
    for (auto it = found->begin(); it != found->end(); ++it) {
      const auto combo = parseKeyCombo(it.key());
      std::string error;
      if (!combo || !it.value().is_string() ||
          !validate_(it.value().get<std::string>(), &error)) {
        logWarning("Key binding %s ignored: %s", it.key().c_str(),
                   error.empty() ? "not a key" : error.c_str());
        continue;
      }
      bindings_[mode][combo->canonical] = it.value();
    }
  }
  index();
}

std::string KeyBindings::actionFor(std::string_view mode, xkb_keysym_t symbol,
                                   uint32_t modifiers) const {
  const auto lower = xkb_keysym_to_lower(symbol);
  for (const auto &entry : entries_)
    if (entry.mode == mode && entry.combo.modifiers == modifiers &&
        xkb_keysym_to_lower(entry.combo.symbol) == lower)
      return entry.action;
  return {};
}

bool KeyBindings::bind(std::string_view mode, std::string_view keys,
                       std::string_view action, std::string_view replace,
                       std::string *error) {
  if (!knownMode(mode)) {
    setError(error, "Unknown key binding mode: " + std::string(mode));
    return false;
  }
  const auto combo = parseKeyCombo(keys);
  if (!combo) {
    setError(error, "Not a key: " + std::string(keys));
    return false;
  }
  if (reservedForSwitcher(*combo)) {
    setError(error, "Alt+Tab is reserved for the window switcher.");
    return false;
  }
  if (!validate_(action, error))
    return false;
  auto next = bindings_;
  auto &bindings = next[std::string(mode)];
  if (!replace.empty())
    if (const auto previous = parseKeyCombo(replace))
      bindings.erase(previous->canonical);
  if (bindings.contains(combo->canonical) &&
      bindings[combo->canonical] != action) {
    setError(error, combo->canonical + " is already bound to " +
                        bindings[combo->canonical].get<std::string>() + ".");
    return false;
  }
  bindings[combo->canonical] = std::string(action);
  return store(std::move(next), error);
}

bool KeyBindings::unbind(std::string_view mode, std::string_view keys,
                         std::string *error) {
  const auto combo = parseKeyCombo(keys);
  if (!knownMode(mode) || !combo ||
      !bindings_[std::string(mode)].contains(combo->canonical)) {
    setError(error, "Nothing is bound to " + std::string(keys) + ".");
    return false;
  }
  auto next = bindings_;
  next[std::string(mode)].erase(combo->canonical);
  return store(std::move(next), error);
}

bool KeyBindings::reset(std::string *error) {
  if (!clearKeyBindings(error))
    return false;
  bindings_ = defaults();
  index();
  return true;
}

bool KeyBindings::store(nlohmann::json next, std::string *error) {
  if (!saveKeyBindings(next, error))
    return false;
  bindings_ = std::move(next);
  index();
  return true;
}

void KeyBindings::index() {
  entries_.clear();
  for (const auto &mode : modes()) {
    const auto found = bindings_.find(mode);
    if (found == bindings_.end())
      continue;
    for (auto it = found->begin(); it != found->end(); ++it)
      if (const auto combo = parseKeyCombo(it.key()); combo && it.value().is_string())
        entries_.push_back({mode, *combo, it.value().get<std::string>()});
  }
}

} // namespace LunaDash
