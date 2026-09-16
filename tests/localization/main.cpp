#include <LuDash/localization/JsonTranslator.h>
#include <LuDash/localization/Localization.h>
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

int main(int argc, char** argv) {
    QCoreApplication application(argc, argv);
    if (argc != 2) return 2;
    qputenv("LUDASH_LANGUAGE", "zh_TW");
    LuDash::initializeLocalization(application);
    const QDir root(QString::fromLocal8Bit(argv[1]));
    const QDir extras(root.filePath("zh_TW"));
    QStringList paths{root.filePath("zh_TW.json")};
    for (const auto& file : extras.entryList({"*.json"}, QDir::Files, QDir::Name))
        paths.append(extras.filePath(file));
    QJsonObject expected;
    for (const auto& path : paths) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) return 3;
        const auto catalog = QJsonDocument::fromJson(file.readAll()).object();
        if (catalog.isEmpty()) return 4;
        for (auto it = catalog.constBegin(); it != catalog.constEnd(); ++it)
            expected.insert(it.key(), it.value());
    }
    const auto dictionary = LuDash::languageDictionary("zh_TW");
    if (dictionary != expected || !dictionary.contains("Input method")) {
        qCritical() << "Embedded catalogs differ from source catalogs";
        return 5;
    }
    for (auto it = expected.constBegin(); it != expected.constEnd(); ++it) {
        const auto source = it.key().toUtf8();
        if (LuDash::translate(source.constData()) != it.value().toString()) {
            qCritical() << "Native translation mismatch:" << it.key();
            return 6;
        }
    }
    LuDash::JsonTranslator english("en_US", &application);
    if (!english.isEmpty() || !LuDash::languageDictionary("unsupported").isEmpty()) return 7;
    if (!english.translate("LuDash", "Input method", nullptr, -1).isEmpty()) return 8;
    if (LuDash::translate("Unknown application name") != QStringLiteral("Unknown application name")) return 9;
    qInfo() << "Verified" << expected.size() << "native and shell dictionary entries, with English fallback";
    return 0;
}
