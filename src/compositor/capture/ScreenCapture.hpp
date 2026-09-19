#pragma once
#include <QObject>
#include <QProcess>
#include <QProcessEnvironment>
#include <QTemporaryFile>
#include <QTimer>
#include <memory>

namespace LunaDash {
class ScreenCapture final : public QObject {
  Q_OBJECT
public:
  explicit ScreenCapture(QObject *parent = nullptr);
  ~ScreenCapture() override;
  bool selectRegion(const QProcessEnvironment &environment, const QString &path,
                    QString *error);
  bool busy() const;
  QString phase() const;
signals:
  void completed(const QString &path, const QString &error);

private:
  QProcess process_;
  QTimer timeout_;
  QProcessEnvironment environment_;
  std::unique_ptr<QTemporaryFile> temporary_;
  QString destination_;
  QString grim_;
  QString phase_ = "idle";
  void finish(const QString &path, const QString &error);
  void processFinished(int code, QProcess::ExitStatus status);
};
} // namespace LunaDash
