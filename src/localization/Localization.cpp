#include <LuDash/localization/JsonTranslator.h>
#include <LuDash/localization/Localization.h>
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

// The generated Qt resource initializers must be called from the global namespace.
int qInitResources_translations();
int qInitResources_desktop_translations();
namespace LuDash {
JsonTranslator::JsonTranslator(const QString& language, QObject* parent) : QTranslator(parent) {
    messages_ = languageDictionary(language);
}
QString JsonTranslator::translate(const char* context, const char* source, const char*, int) const {
    if (QByteArray(context) != "LuDash") return {};
    return messages_.value(QString::fromUtf8(source)).toString();
}
bool JsonTranslator::isEmpty() const { return messages_.isEmpty(); }
QJsonObject languageDictionary(const QString& language) {
    if (language != "en_US" && language != "zh_TW") return {};
    const QString root = QStringLiteral(":/LuDash/data/translations/");
    QStringList paths{root + language + ".json"};
    const QDir features(root + language);
    for (const auto& name : features.entryList({"*.json"}, QDir::Files, QDir::Name))
        paths.append(features.filePath(name));
    QJsonObject messages;
    for (const auto& path : paths) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            qWarning() << "Could not load translation catalog:" << path;
            continue;
        }
        QJsonParseError error;
        const auto document = QJsonDocument::fromJson(file.readAll(), &error);
        if (error.error != QJsonParseError::NoError || !document.isObject()) {
            qWarning() << "Invalid translation catalog:" << path << error.errorString();
            continue;
        }
        const auto catalog = document.object();
        for (auto it = catalog.constBegin(); it != catalog.constEnd(); ++it) {
            if (!it.value().isString() || it.value().toString().isEmpty()) continue;
            if (messages.contains(it.key())) {
                qWarning() << "Duplicate translation key:" << path << it.key();
                continue;
            }
            messages.insert(it.key(), it.value());
        }
    }
    return messages;
}
QString translate(const char* source) { return QCoreApplication::translate("LuDash", source); }
QString selectedLanguage() {
    QString locale = qEnvironmentVariable("LUDASH_LANGUAGE");
    if (locale.isEmpty()) locale = QSettings().value("appearance/language", QLocale::system().name()).toString();
    return locale.startsWith("zh") ? "zh_TW" : "en_US";
}
void initializeLocalization(QCoreApplication& application) {
    ::qInitResources_translations();
    ::qInitResources_desktop_translations();
    const auto language = selectedLanguage();
    auto* translator = new JsonTranslator(language, &application);
    application.installTranslator(translator);
    auto* qtTranslator = new QTranslator(&application);
    if (qtTranslator->load("qtbase_" + language, QLibraryInfo::path(QLibraryInfo::TranslationsPath))) application.installTranslator(qtTranslator);
}
}
