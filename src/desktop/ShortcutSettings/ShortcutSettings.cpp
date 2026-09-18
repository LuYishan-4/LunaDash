#include "desktop/ShortcutSettings/ShortcutSettings.hpp"

#include <QSet>
#include <QSettings>
#include <QStringList>

namespace LuDash {
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
  result.insert("toggleFloating", "Meta+Space");
  result.insert("launchTerminal", "Meta+Return");
  result.insert("launchFiles", "Meta+E");
  result.insert("launchLauncher", "Meta+D");
  result.insert("screenshot", "Alt+Shift+F5");
  for (int workspace = 1; workspace <= 9; ++workspace) {
    result.insert(QString("workspace%1").arg(workspace),
                  QString("Meta+%1").arg(workspace));
    result.insert(QString("moveToWorkspace%1").arg(workspace),
                  QString("Meta+Shift+%1").arg(workspace));
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
  const auto configured =
      QJsonObject::fromVariantMap(settings.value("shortcuts/bindings").toMap());
  QString error;
  if (!configured.isEmpty())
    apply(configured, &error);
}

QJsonObject ShortcutSettings::snapshot() const { return bindings_; }

QString ShortcutSettings::actionFor(xkb_keysym_t keysym,
                                    uint32_t modifiers) const {
  for (auto it = bindings_.cbegin(); it != bindings_.cend(); ++it) {
    const ParsedShortcut shortcut = parseShortcut(it.value().toString());
    if (shortcut.canonical != "Disabled" && shortcut.symbol == keysym &&
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
    candidate.insert(it.key(), parsed.canonical);
  }

  QSet<QString> used;
  for (auto it = candidate.cbegin(); it != candidate.cend(); ++it) {
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

} // namespace LuDash
