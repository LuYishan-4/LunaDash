#include "desktop/shortcuts/ShortcutSettings.hpp"

#include <QSet>
#include <QSettings>
#include <QStringList>

namespace LunaDash {
namespace {

QJsonObject defaults() {
  QJsonObject result;
  result.insert("focusLeft", "Meta+H");
  result.insert("focusRight", "Meta+L");
  result.insert("focusUp", "Meta+K");
  result.insert("focusDown", "Meta+J");
  result.insert("groupLeft", "Meta+Shift+H");
  result.insert("groupRight", "Meta+Shift+L");
  result.insert("reorderLeft", "Meta+Ctrl+H");
  result.insert("reorderRight", "Meta+Ctrl+L");
  result.insert("expelWindow", "Meta+Shift+E");
  result.insert("centerColumn", "Meta+Shift+C");
  result.insert("widenColumn", "Meta+=");
  result.insert("narrowColumn", "Meta+-");
  result.insert("maximizeWindow", "Meta+F");
  result.insert("closeWindow", "Meta+C");
  result.insert("minimizeWindow", "Meta+M");
  result.insert("closeWindowAlternate", "Meta+Q");
  result.insert("launchTerminal", "Meta+T");
  result.insert("launchTerminalAlternate", "Meta+Return");
  result.insert("launchFiles", "Meta+E");
  result.insert("launchLauncher", "Meta+D");
  result.insert("screenshot", "Meta+Shift+S");
  result.insert("launchOrbit", "Meta+A");
  result.insert("chooseWallpaper", "Meta+W");
  result.insert("randomWallpaper", "Meta+Ctrl+W");
  result.insert("toggleEyeCare", "Meta+N");
  result.insert("toggleScratchpad", "Meta+grave");
  result.insert("openControlCenter", "Meta+I");
  result.insert("openClipboard", "Meta+V");
  result.insert("openPowerMenu", "Meta+X");
  result.insert("toggleFloating", "Meta+Shift+T");
  result.insert("toggleFullscreen", "Meta+Shift+F");
  for (int workspace = 1; workspace <= 10; ++workspace) {
    result.insert(QString("workspace%1").arg(workspace),
                  QString("Meta+%1").arg(workspace % 10));
    result.insert(QString("moveToWorkspace%1").arg(workspace),
                  QString("Meta+Shift+%1").arg(workspace % 10));
  }
  return result;
}

xkb_keysym_t keySym(const QString &name) {
  if (name == "=")
    return XKB_KEY_equal;
  if (name == "-")
    return XKB_KEY_minus;
  if (name.compare("Space", Qt::CaseInsensitive) == 0)
    return XKB_KEY_space;
  if (name.compare("Return", Qt::CaseInsensitive) == 0)
    return XKB_KEY_Return;
  const QByteArray utf8 = name.toLatin1();
  return xkb_keysym_from_name(utf8.constData(), XKB_KEYSYM_CASE_INSENSITIVE);
}

struct ParsedShortcut {
  xkb_keysym_t symbol = XKB_KEY_NoSymbol;
  uint32_t modifiers = 0;
  QString canonical;
};

ParsedShortcut parseShortcut(const QString &text) {
  if (text == "Disabled")
    return {XKB_KEY_NoSymbol, 0, QStringLiteral("Disabled")};

  const QStringList pieces = text.split('+', Qt::KeepEmptyParts);
  if (pieces.size() < 2)
    return {};

  uint32_t modifiers = 0;
  QString key;
  for (const QString &piece : pieces) {
    if (piece == "Meta")
      modifiers |= ShortcutMeta;
    else if (piece == "Ctrl" || piece == "Control")
      modifiers |= ShortcutControl;
    else if (piece == "Alt")
      modifiers |= ShortcutAlt;
    else if (piece == "Shift")
      modifiers |= ShortcutShift;
    else if (key.isEmpty())
      key = piece;
    else
      return {};
  }

  if (key.isEmpty() || !(modifiers & (ShortcutMeta | ShortcutAlt)))
    return {};

  const xkb_keysym_t symbol = keySym(key);
  if (symbol == XKB_KEY_NoSymbol)
    return {};

  QString canonicalKey = key;
  if (key.size() == 1 && key.front().isLetter())
    canonicalKey = key.toUpper();
  else if (symbol == XKB_KEY_space)
    canonicalKey = QStringLiteral("Space");
  else if (symbol == XKB_KEY_Return)
    canonicalKey = QStringLiteral("Return");

  QStringList normalized;
  if (modifiers & ShortcutMeta)
    normalized << QStringLiteral("Meta");
  if (modifiers & ShortcutControl)
    normalized << QStringLiteral("Ctrl");
  if (modifiers & ShortcutAlt)
    normalized << QStringLiteral("Alt");
  if (modifiers & ShortcutShift)
    normalized << QStringLiteral("Shift");
  normalized << canonicalKey;
  return {symbol, modifiers, normalized.join('+')};
}

} // namespace

ShortcutSettings::ShortcutSettings() : bindings_(defaults()) {
  QSettings settings;
  auto configured =
      QJsonObject::fromVariantMap(settings.value("shortcuts/bindings").toMap());

  // Older settings may reserve Meta+T for a different action. Keep those
  // assignments; add the terminal defaults only when their keys are free.
  const auto uses = [&configured](const QString &sequence,
                                  const QString &except) {
    for (auto it = configured.begin(); it != configured.end(); ++it)
      if (it.key() != except && it.value().toString() == sequence)
        return true;
    return false;
  };
  if (configured.value("launchTerminal").toString() == "Meta+Return" &&
      !uses("Meta+T", "launchTerminal"))
    configured["launchTerminal"] = "Meta+T";
  if (!configured.contains("launchTerminal") &&
      uses("Meta+T", "launchTerminal"))
    bindings_["launchTerminal"] = "Disabled";
  if (!configured.contains("launchTerminalAlternate") &&
      uses("Meta+Return", "launchTerminalAlternate"))
    bindings_["launchTerminalAlternate"] = "Disabled";
  if (!configured.contains("workspace10") && uses("Meta+0", "workspace10"))
    bindings_["workspace10"] = "Disabled";
  if (!configured.contains("moveToWorkspace10") &&
      uses("Meta+Shift+0", "moveToWorkspace10"))
    bindings_["moveToWorkspace10"] = "Disabled";
  for (auto it = configured.begin(); it != configured.end(); ++it) {
    const auto binding = parseShortcut(it.value().toString());
    if ((binding.symbol == XKB_KEY_Tab ||
         binding.symbol == XKB_KEY_ISO_Left_Tab) &&
        (binding.modifiers == ShortcutAlt ||
         binding.modifiers == (ShortcutAlt | ShortcutShift) ||
         binding.modifiers == ShortcutMeta ||
         binding.modifiers == (ShortcutMeta | ShortcutShift)))
      it.value() = "Disabled";
  }
  // Migrate the previous shipped default without replacing custom bindings
  // or stealing Meta+Shift+S from another explicitly assigned action.
  if (configured.value("screenshot").toString() == "Alt+Shift+F5") {
    bool used = false;
    for (auto it = configured.begin(); it != configured.end(); ++it)
      used |=
          it.key() != "screenshot" && it.value().toString() == "Meta+Shift+S";
    if (!used)
      configured["screenshot"] = "Meta+Shift+S";
  }
  // New defaults never invalidate a user's existing custom bindings.
  for (auto it = bindings_.begin(); it != bindings_.end(); ++it)
    if (!configured.contains(it.key()) && uses(it.value().toString(), it.key()))
      it.value() = "Disabled";
  QString error;
  if (!configured.isEmpty())
    apply(configured, &error);
}

QJsonObject ShortcutSettings::snapshot() const { return bindings_; }

QString ShortcutSettings::actionFor(xkb_keysym_t keysym,
                                    uint32_t modifiers) const {
  for (auto it = bindings_.begin(); it != bindings_.end(); ++it) {
    const ParsedShortcut shortcut = parseShortcut(it.value().toString());
    if (shortcut.canonical != "Disabled" &&
        xkb_keysym_to_lower(shortcut.symbol) == xkb_keysym_to_lower(keysym) &&
        shortcut.modifiers == modifiers)
      return it.key();
  }
  return {};
}

bool ShortcutSettings::apply(const QJsonObject &changes, QString *error) {
  auto candidate = bindings_;
  const auto known = defaults();
  for (auto it = changes.begin(); it != changes.end(); ++it) {
    if (!known.contains(it.key()) || !it.value().isString()) {
      if (error)
        *error = "Unknown shortcut action: " + it.key();
      return false;
    }
    const ParsedShortcut parsed = parseShortcut(it.value().toString());
    if (parsed.canonical.isEmpty()) {
      if (error)
        *error = "Shortcut must be one Meta or Alt key combination.";
      return false;
    }
    if ((parsed.symbol == XKB_KEY_Tab ||
         parsed.symbol == XKB_KEY_ISO_Left_Tab) &&
        (parsed.modifiers == ShortcutAlt ||
         parsed.modifiers == (ShortcutAlt | ShortcutShift) ||
         parsed.modifiers == ShortcutMeta ||
         parsed.modifiers == (ShortcutMeta | ShortcutShift))) {
      if (error)
        *error = "Alt+Tab and Super+Tab are reserved for window and workspace switching.";
      return false;
    }
    candidate.insert(it.key(), parsed.canonical);
  }

  QSet<QString> used;
  for (auto it = candidate.begin(); it != candidate.end(); ++it) {
    const QString sequence = it.value().toString();
    if (sequence == "Disabled")
      continue;
    if (used.contains(sequence)) {
      if (error)
        *error = "Shortcut is already assigned: " + sequence;
      return false;
    }
    used.insert(sequence);
  }

  bindings_ = candidate;
  QSettings settings;
  settings.setValue("shortcuts/bindings", bindings_.toVariantMap());
  settings.sync();
  if (settings.status() != QSettings::NoError) {
    if (error)
      *error = "Could not save shortcut settings.";
    return false;
  }
  return true;
}

void ShortcutSettings::reset() {
  bindings_ = defaults();
  QSettings settings;
  settings.remove("shortcuts/bindings");
  settings.sync();
}

} // namespace LunaDash
