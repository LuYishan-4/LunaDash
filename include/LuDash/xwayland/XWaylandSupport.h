#pragma once
#include <QJsonObject>
#include <QObject>
#include <QProcess>
#include <QProcessEnvironment>
#include <QSize>
#include <QTemporaryDir>
#include <memory>
namespace LuDash {
class XWaylandSupport final : public QObject {
public:
  explicit XWaylandSupport(QObject *parent = nullptr);
  ~XWaylandSupport() override;
  bool start(const QProcessEnvironment &environment,
             const QSize &screenSize = QSize(1440, 900));
  void stop();
  bool stopped() const;
  bool startServer(QString *error = nullptr);
  QJsonObject snapshot() const;
  void applyEnvironment(QProcessEnvironment &environment) const;
  bool launch(const QStringList &command, QString *error);

private:
  void releaseSocket();
  QProcess server_;
  QList<QProcess *> clients_;
  QProcessEnvironment environment_;
  std::unique_ptr<QTemporaryDir> runtime_;
  QString display_;
  QString socketPath_;
  QString authority_;
  QString error_;
  int descriptor_ = -1;
  qint64 groupId_ = 0;
  bool stopping_ = false;
};
} // namespace LuDash
