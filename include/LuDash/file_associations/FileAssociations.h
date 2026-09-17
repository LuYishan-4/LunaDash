#pragma once
#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>

namespace LuDash {
struct FileApplication {
    QString id;
    QString name;
    QString icon;
    bool recommended = false;
    bool systemDefault = false;
};

// Extension rules are private to Files; changing a system MIME default is a
// separate, explicit operation because several extensions may share one MIME.
class FileAssociations final {
public:
    explicit FileAssociations(QString path = {});
    QString path() const;
    QJsonObject read(QString* error = nullptr) const;
    QString keyForFile(const QString& path) const;
    QString applicationForFile(const QString& path, QString* error = nullptr) const;
    bool setPreferences(bool initialized, bool askOnFirstOpen, QString* error = nullptr) const;
    bool setRule(const QString& key, const QString& desktopId, const QString& mimeType,
                 QString* error = nullptr) const;
    bool removeRule(const QString& key, QString* error = nullptr) const;
    static QString keyForExtension(const QString& extension);
    static QString mimeTypeForFile(const QString& path);
    static bool validKey(const QString& key);
private:
    bool update(const QString& key, const QJsonObject& value, bool preferences,
                QString* error) const;
    QString path_;
};

QList<FileApplication> fileApplications(const QString& mimeType);
bool fileApplicationAvailable(const QString& desktopId);
QString systemFileApplication(const QString& mimeType);
bool setSystemFileApplication(const QString& desktopId, const QString& mimeType,
                              QString* error = nullptr);
// Uses GIO's desktop-entry launcher, not a shell or a hand-written Exec parser.
bool launchFilesWithApplication(const QString& desktopId, const QStringList& files,
                                QString* error = nullptr);
} // namespace LuDash
