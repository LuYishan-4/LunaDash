#include <LuDash/localization/JsonTranslator.h>
#include <LuDash/localization/Localization.h>
#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLibraryInfo>
#include <QLocale>
#include <QSettings>
#include <QTranslator>

// The generated Qt resource initializer must be called from the global namespace.
int qInitResources_translations();
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
    QFile file(":/LuDash/data/translations/" + language + ".json");
    if (file.open(QIODevice::ReadOnly)) return QJsonDocument::fromJson(file.readAll()).object();
    return {};
}
QString translate(const char* source) { return QCoreApplication::translate("LuDash", source); }
QString selectedLanguage() {
    QString locale = qEnvironmentVariable("LUDASH_LANGUAGE");
    if (locale.isEmpty()) locale = QSettings().value("appearance/language", QLocale::system().name()).toString();
    return locale.startsWith("zh") ? "zh_TW" : "en_US";
}
void initializeLocalization(QCoreApplication& application) {
    ::qInitResources_translations();
    const auto language = selectedLanguage();
    auto* translator = new JsonTranslator(language, &application);
    application.installTranslator(translator);
    auto* qtTranslator = new QTranslator(&application);
    if (qtTranslator->load("qtbase_" + language, QLibraryInfo::path(QLibraryInfo::TranslationsPath))) application.installTranslator(qtTranslator);
}
}
