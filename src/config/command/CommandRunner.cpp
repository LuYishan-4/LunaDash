#include "config/command/CommandRunner.hpp"
#include <QProcessEnvironment>
#include <algorithm>
namespace LunaDash {
CommandRunner::CommandRunner(QObject *parent) : QObject(parent) {
  process_.setProcessChannelMode(QProcess::MergedChannels);
  timeout_.setSingleShot(true);
  timeout_.setInterval(2500);
  connect(&timeout_, &QTimer::timeout, this, [this] {
    failed_ = true;
    process_.kill();
  });
  connect(&process_, &QProcess::readyRead, this, [this] {
    output_ += process_.read(65537 - output_.size());
    if (output_.size() > 65536) {
      failed_ = true;
      process_.kill();
    }
  });
  connect(&process_, &QProcess::finished, this,
          [this](int code, QProcess::ExitStatus status) {
            finish(!failed_ && code == 0 && status == QProcess::NormalExit);
          });
  connect(&process_, &QProcess::errorOccurred, this,
          [this](QProcess::ProcessError error) {
            if (error == QProcess::FailedToStart)
              finish(false);
          });
}
CommandRunner::~CommandRunner() {
  completion_ = {};
  disconnect(&process_, nullptr, this, nullptr);
  if (process_.state() != QProcess::NotRunning) {
    process_.kill();
    process_.waitForFinished(1000);
  }
}
bool CommandRunner::busy() const { return bool(completion_); }
bool CommandRunner::run(const QString &program, const QStringList &arguments,
                        Completion completion, int timeoutMilliseconds) {
  if (busy() || program.isEmpty() || !completion)
    return false;
  output_.clear();
  failed_ = false;
  completion_ = std::move(completion);
  auto environment = QProcessEnvironment::systemEnvironment();
  environment.insert("LC_ALL", "C");
  process_.setProcessEnvironment(environment);
  timeout_.setInterval(std::clamp(timeoutMilliseconds, 1, 60000));
  process_.start(program, arguments);
  timeout_.start();
  return true;
}
void CommandRunner::finish(bool success) {
  timeout_.stop();
  output_ += process_.read(65537 - output_.size());
  auto callback = std::move(completion_);
  completion_ = {};
  if (callback)
    callback(success && output_.size() <= 65536, output_.left(65536));
}
} // namespace LunaDash
