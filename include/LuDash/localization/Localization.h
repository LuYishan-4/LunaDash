#pragma once
#include <QString>
#include <QJsonObject>
class QCoreApplication;
namespace LuDash {
QString translate(const char* source);
QString selectedLanguage();
QJsonObject languageDictionary(const QString& language);
void initializeLocalization(QCoreApplication& application);
}
