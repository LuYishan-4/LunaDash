#pragma once
#include <QFileSystemWatcher>
#include <QJsonObject>
#include <QObject>
#include <QTimer>
namespace LunaDash {
class ShellModules final : public QObject {
  Q_OBJECT
public:
  explicit ShellModules(QObject *parent = nullptr);
  QJsonObject snapshot() const;
  bool validate(const QByteArray &text, QString *error);
  bool apply(const QByteArray &text, QString *error);
  bool reset(QString *error);
  void reportError(const QString &id, const QString &error);
  int panelExtent(int fallbackHeight) const;
  QString panelEdge() const;
  bool panelAtBottom() const;
signals:
  void changed();

private:
  QString path_, status_;
  QJsonObject document_, errors_;
  QFileSystemWatcher watcher_;
  QTimer debounce_;
  int revision_ = 0;
  void reload();
  void watchEntries();
};
} // namespace LunaDash
