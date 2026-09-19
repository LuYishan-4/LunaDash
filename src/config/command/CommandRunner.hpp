#pragma once
#include <QObject>
#include <QProcess>
#include <QTimer>
#include <functional>
namespace LunaDash {
class CommandRunner final : public QObject {
public:
  using Completion = std::function<void(bool, const QByteArray &)>;
  explicit CommandRunner(QObject *parent = nullptr);
  ~CommandRunner() override;
  bool run(const QString &program, const QStringList &arguments,
           Completion completion);
  bool busy() const;

private:
  QProcess process_;
  QTimer timeout_;
  QByteArray output_;
  Completion completion_;
  bool failed_ = false;
  void finish(bool success);
};
} // namespace LunaDash
