#pragma once
#include <QString>
#include <QJsonObject>
class QCoreApplication;
namespace LuDash {
QString translate(const char* source);
QString selectedLanguage();
bool isSupportedLanguage(const QString& language);
QJsonObject languageDictionary(const QString& language);
void initializeLocalization(QCoreApplication& application);
}
