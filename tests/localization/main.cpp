#include "config/JsonTranslator/JsonTranslator.hpp"
#include "config/Localization/Localization.hpp"
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

namespace {
QJsonObject sourceDictionary(const QDir &root, const QString &locale) {
    QStringList paths;
    const QString rootCatalog = root.filePath(locale + ".json");
    if (QFileInfo::exists(rootCatalog))
        paths.append(rootCatalog);
    const QDir extras(root.filePath(locale));
    for (const auto &file : extras.entryList({"*.json"}, QDir::Files, QDir::Name))
        paths.append(extras.filePath(file));

    QJsonObject expected;
    for (const auto &path : paths) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly))
            return {};
        QJsonParseError error;
        const auto document = QJsonDocument::fromJson(file.readAll(), &error);
        if (error.error != QJsonParseError::NoError || !document.isObject())
            return {};
        const auto catalog = document.object();
        for (auto it = catalog.constBegin(); it != catalog.constEnd(); ++it)
            expected.insert(it.key(), it.value());
    }
    return expected;
}
} // namespace

int main(int argc, char** argv) {
    QCoreApplication application(argc, argv);
    if (argc != 2)
        return 2;

    qputenv("LUDASH_LANGUAGE", "zh_TW");
    LuDash::initializeLocalization(application);
    const QDir root(QString::fromLocal8Bit(argv[1]));
    const QRegularExpression localePattern(QStringLiteral("^[a-z]{2,3}_[A-Z]{2}\\.json$"));

    QStringList locales;
    for (const auto &file : root.entryList({"*.json"}, QDir::Files, QDir::Name)) {
        if (!localePattern.match(file).hasMatch())
            continue;
        const QString locale = QFileInfo(file).completeBaseName();
        if (locale != QStringLiteral("en_US"))
            locales.append(locale);
    }
    if (!locales.contains(QStringLiteral("zh_TW")) ||
        !locales.contains(QStringLiteral("zh_CN")) ||
        !locales.contains(QStringLiteral("ja_JP"))) {
        qCritical() << "Expected shipped locales are missing:" << locales;
        return 3;
    }

    for (const auto &locale : locales) {
        const auto expected = sourceDictionary(root, locale);
        if (expected.isEmpty()) {
            qCritical() << "Could not read source catalogs for" << locale;
            return 4;
        }
        if (!LuDash::isSupportedLanguage(locale)) {
            qCritical() << "Packaged locale is not detected:" << locale;
            return 5;
        }
        const auto embedded = LuDash::languageDictionary(locale);
        if (embedded != expected) {
            qCritical() << "Embedded catalogs differ from source catalogs for" << locale
                        << "embedded:" << embedded.size() << "source:" << expected.size();
            return 6;
        }

        LuDash::JsonTranslator translator(locale, &application);
        for (auto it = expected.constBegin(); it != expected.constEnd(); ++it) {
            const auto source = it.key().toUtf8();
            if (translator.translate("LuDash", source.constData(), nullptr, -1) != it.value().toString()) {
                qCritical() << "Translator mismatch:" << locale << it.key();
                return 7;
            }
        }
        qInfo() << "Verified locale" << locale << "with" << expected.size() << "entries";
    }

    const auto traditional = sourceDictionary(root, QStringLiteral("zh_TW"));
    for (auto it = traditional.constBegin(); it != traditional.constEnd(); ++it) {
        const auto source = it.key().toUtf8();
        if (LuDash::translate(source.constData()) != it.value().toString()) {
            qCritical() << "Installed native translation mismatch:" << it.key();
            return 8;
        }
    }

    LuDash::JsonTranslator english("en_US", &application);
    if (!english.isEmpty() || !LuDash::languageDictionary("unsupported").isEmpty())
        return 9;
    if (!english.translate("LuDash", "Input method", nullptr, -1).isEmpty())
        return 10;
    if (!LuDash::isSupportedLanguage("en_US") || LuDash::isSupportedLanguage("unsupported"))
        return 11;
    if (LuDash::translate("Unknown application name") != QStringLiteral("Unknown application name"))
        return 12;

    qInfo() << "Verified" << locales.size()
            << "packaged locale dictionaries with English/source fallback";
    return 0;
}
