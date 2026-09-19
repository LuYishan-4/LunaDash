#pragma once
#include <QJsonObject>
#include <QString>
class QCoreApplication;
namespace LunaDash {
QString translate(const char *source);
QString selectedLanguage();
bool isSupportedLanguage(const QString &language);
QJsonObject languageDictionary(const QString &language);
void initializeLocalization(QCoreApplication &application);
} // namespace LunaDash
