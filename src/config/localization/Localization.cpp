#include "config/localization/Localization.hpp"
#include "config/localization/JsonTranslator.hpp"
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLibraryInfo>
#include <QLocale>
#include <QSettings>
#include <QTranslator>

// The generated Qt resource initializers must be called from the global
// namespace.
int qInitResources_translations();
int qInitResources_desktop_translations();
namespace LunaDash {
namespace {
const QString kTranslationRoot = QStringLiteral(":/LuDash/data/translations/");

QString normalizedLocale(QString locale) {
  locale = locale.trimmed();
  locale.replace('-', '_');
  return locale;
}

QString canonicalLanguage(QString locale) {
  locale = normalizedLocale(locale);
  if (isSupportedLanguage(locale))
    return locale;

  const QString lower = locale.toLower();
  if ((lower.startsWith("zh_cn") || lower.startsWith("zh_hans")) &&
      isSupportedLanguage(QStringLiteral("zh_CN")))
    return QStringLiteral("zh_CN");
  if (lower.startsWith("zh") && isSupportedLanguage(QStringLiteral("zh_TW")))
    return QStringLiteral("zh_TW");
  if (lower.startsWith("ja") && isSupportedLanguage(QStringLiteral("ja_JP")))
    return QStringLiteral("ja_JP");
  if (lower.startsWith("en"))
    return QStringLiteral("en_US");
  return QStringLiteral("en_US");
}
} // namespace

bool isSupportedLanguage(const QString &language) {
  const QString locale = normalizedLocale(language);
  if (locale == QStringLiteral("en_US"))
    return true;
  if (locale.isEmpty())
    return false;
  return QFile::exists(kTranslationRoot + locale + QStringLiteral(".json")) ||
         QDir(kTranslationRoot + locale).exists();
}

QJsonObject languageDictionary(const QString &language) {
  const QString locale = normalizedLocale(language);
  if (!isSupportedLanguage(locale) || locale == QStringLiteral("en_US"))
    return {};

  QStringList paths;
  const QString rootCatalog =
      kTranslationRoot + locale + QStringLiteral(".json");
  if (QFile::exists(rootCatalog))
    paths.append(rootCatalog);
  const QDir features(kTranslationRoot + locale);
  for (const auto &name :
       features.entryList({"*.json"}, QDir::Files, QDir::Name))
    paths.append(features.filePath(name));

  QJsonObject messages;
  for (const auto &path : paths) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
      qWarning() << "Could not load translation catalog:" << path;
      continue;
    }
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
      qWarning() << "Invalid translation catalog:" << path
                 << error.errorString();
      continue;
    }
    const auto catalog = document.object();
    for (auto it = catalog.constBegin(); it != catalog.constEnd(); ++it) {
      if (!it.value().isString() || it.value().toString().isEmpty())
        continue;
      if (messages.contains(it.key())) {
        qWarning() << "Duplicate translation key:" << path << it.key();
        continue;
      }
      messages.insert(it.key(), it.value());
    }
  }
  return messages;
}

QString translate(const char *source) {
  return QCoreApplication::translate("LuDash", source);
}

QString selectedLanguage() {
  QString locale = qEnvironmentVariable("LUDASH_LANGUAGE");
  if (locale.isEmpty())
    locale = QSettings()
                 .value("appearance/language", QLocale::system().name())
                 .toString();
  return canonicalLanguage(locale);
}

void initializeLocalization(QCoreApplication &application) {
  ::qInitResources_translations();
  ::qInitResources_desktop_translations();
  const auto language = selectedLanguage();
  auto *translator = new JsonTranslator(language, &application);
  application.installTranslator(translator);
  auto *qtTranslator = new QTranslator(&application);
  if (qtTranslator->load("qtbase_" + language,
                         QLibraryInfo::path(QLibraryInfo::TranslationsPath)))
    application.installTranslator(qtTranslator);
}
} // namespace LunaDash
