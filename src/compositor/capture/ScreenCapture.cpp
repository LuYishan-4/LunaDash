#include "compositor/capture/ScreenCapture.hpp"
#include <QFileInfo>
#include <QRegularExpression>
#include <QStandardPaths>

namespace LunaDash {
ScreenCapture::ScreenCapture(QObject *parent) : QObject(parent) {
  timeout_.setSingleShot(true);
  timeout_.setInterval(15000);
  connect(&timeout_, &QTimer::timeout, this, [this] {
    process_.kill();
    finish({}, "Screenshot capture timed out.");
  });
  connect(&process_, &QProcess::finished, this,
          &ScreenCapture::processFinished);
  connect(&process_, &QProcess::errorOccurred, this,
          [this](QProcess::ProcessError error) {
            if (error == QProcess::FailedToStart)
              finish({}, "Could not start the screenshot helper.");
          });
}
ScreenCapture::~ScreenCapture() {
  disconnect(&process_, nullptr, this, nullptr);
  if (process_.state() != QProcess::NotRunning) {
    process_.kill();
    process_.waitForFinished(1000);
  }
}
bool ScreenCapture::busy() const {
  return process_.state() != QProcess::NotRunning || phase_ == "selecting" ||
         phase_ == "capturing";
}
QString ScreenCapture::phase() const { return phase_; }
bool ScreenCapture::selectRegion(const QProcessEnvironment &environment,
                                 const QString &path, QString *error) {
  if (busy()) {
    *error = "A screenshot selection is already active.";
    return false;
  }
  const QString selector = QStandardPaths::findExecutable("slurp");
  grim_ = QStandardPaths::findExecutable("grim");
  if (selector.isEmpty() || grim_.isEmpty()) {
    *error = "Install grim and slurp to select a screenshot region.";
    return false;
  }
  if (!QFileInfo(path).isAbsolute() || QFileInfo::exists(path)) {
    *error = "Screenshot destination must be a new absolute path.";
    return false;
  }
  environment_ = environment;
  destination_ = path;
  phase_ = "selecting";
  process_.setProcessEnvironment(environment_);
  process_.start(selector, {"-f", "%x,%y %wx%h"});
  return true;
}
void ScreenCapture::finish(const QString &path, const QString &error) {
  if (!busy())
    return;
  timeout_.stop();
  phase_ = !error.isEmpty() ? "failed" : path.isEmpty() ? "cancelled" : "saved";
  temporary_.reset();
  emit completed(path, error);
}
void ScreenCapture::processFinished(int code, QProcess::ExitStatus status) {
  if (!busy())
    return;
  const QString output =
      QString::fromUtf8(process_.readAllStandardOutput()).trimmed();
  const QString diagnostic =
      QString::fromUtf8(process_.readAllStandardError()).trimmed().left(1024);
  if (phase_ == "selecting") {
    if (status == QProcess::NormalExit && code == 1 && diagnostic.isEmpty()) {
      finish({}, {});
      return;
    }
    static const QRegularExpression geometry(
        "^-?[0-9]+,-?[0-9]+ ([1-9][0-9]*)x([1-9][0-9]*)$");
    const auto match = geometry.match(output);
    if (status != QProcess::NormalExit || code != 0 || !match.hasMatch() ||
        match.captured(1).toLongLong() > 32768 ||
        match.captured(2).toLongLong() > 32768) {
      finish({}, "Could not select a screenshot region." +
                     (diagnostic.isEmpty() ? QString() : " " + diagnostic));
      return;
    }
    temporary_ = std::make_unique<QTemporaryFile>(
        QFileInfo(destination_).absolutePath() + "/.lunadash-capture-XXXXXX");
    if (!temporary_->open()) {
      finish({}, "Could not create the screenshot file.");
      return;
    }
    temporary_->close();
    phase_ = "capturing";
    // slurp has disconnected before grim starts, so the selection overlay is
    // not included in the captured region. The Wayland loop remains live.
    process_.start(grim_, {"-g", output, temporary_->fileName()});
    timeout_.start();
    return;
  }
  if (status != QProcess::NormalExit || code != 0 || !temporary_ ||
      QFileInfo(temporary_->fileName()).size() == 0 ||
      !QFile::rename(temporary_->fileName(), destination_)) {
    finish({}, "Could not save the selected screenshot region." +
                   (diagnostic.isEmpty() ? QString() : " " + diagnostic));
    return;
  }
  finish(destination_, {});
}
} // namespace LunaDash
