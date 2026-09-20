#include "compositor/window/WindowSwitcher.hpp"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSaveFile>
#include <QTimer>
#include <QUrl>
#include <algorithm>

namespace LunaDash {
WindowSwitcher::WindowSwitcher(QObject *parent) : QObject(parent) {}
WindowSwitcher::~WindowSwitcher() {
  if (capture_) {
    capture_->kill();
    capture_->waitForFinished(100);
  }
  if (!channelPath_.isEmpty())
    QFile::remove(channelPath_);
}
void WindowSwitcher::setChannelPath(const QString &path) {
  channelPath_ = path;
  publish();
}
void WindowSwitcher::recordFocus(int window) {
  history_.removeAll(window);
  history_.prepend(window);
}
bool WindowSwitcher::active() const { return active_; }
bool WindowSwitcher::begin(const QJsonArray &windows, int focused,
                           int direction,
                           const QProcessEnvironment &environment) {
  if (active_) {
    step(direction);
    return true;
  }
  if (windows.isEmpty())
    return false;
  windows_ = {};
  for (const auto id : history_)
    for (const auto &entry : windows)
      if (entry.toObject().value("id").toInt() == id)
        windows_.append(entry);
  for (const auto &entry : windows)
    if (!history_.contains(entry.toObject().value("id").toInt()))
      windows_.append(entry);
  index_ = 0;
  for (int i = 0; i < windows_.size(); ++i)
    if (windows_[i].toObject().value("id").toInt() == focused)
      index_ = i;
  active_ = true;
  ready_ = false;
  background_.clear();
  ++serial_;
  step(direction);
  captureBackground(environment);
  return true;
}
void WindowSwitcher::step(int direction) {
  if (!active_ || windows_.isEmpty())
    return;
  const int count = static_cast<int>(windows_.size());
  index_ = (index_ + (direction < 0 ? count - 1 : 1)) % count;
  publish();
}
bool WindowSwitcher::select(int window) {
  if (!active_)
    return false;
  for (int i = 0; i < windows_.size(); ++i) {
    if (windows_[i].toObject().value("id").toInt() != window)
      continue;
    index_ = i;
    publish();
    return true;
  }
  return false;
}
int WindowSwitcher::finish(bool accept) {
  if (!active_)
    return 0;
  const int selected = accept && !windows_.isEmpty()
                           ? windows_[index_].toObject().value("id").toInt()
                           : 0;
  active_ = ready_ = false;
  if (capture_) {
    capture_->kill();
    capture_ = nullptr;
  }
  publish();
  return selected;
}
void WindowSwitcher::remove(int window) {
  history_.removeAll(window);
  for (int i = 0; i < windows_.size(); ++i) {
    if (windows_[i].toObject().value("id").toInt() != window)
      continue;
    windows_.removeAt(i);
    if (i < index_)
      --index_;
    index_ = std::clamp(index_, 0,
                        std::max(0, static_cast<int>(windows_.size()) - 1));
    if (windows_.isEmpty())
      finish(false);
    else
      publish();
    break;
  }
}
void WindowSwitcher::setDrag(const QJsonObject &drag) {
  drag_ = drag;
  publish();
}
QJsonObject WindowSwitcher::snapshot() const {
  return {{"active", active_},   {"ready", ready_},
          {"index", index_},     {"serial", serial_},
          {"windows", windows_}, {"background", background_},
          {"drag", drag_}};
}
void WindowSwitcher::publish() {
  if (channelPath_.isEmpty())
    return;
  const auto data = QJsonDocument(snapshot()).toJson(QJsonDocument::Compact);
  if (data == lastPublished_)
    return;
  QSaveFile file(channelPath_);
  if (!file.open(QIODevice::WriteOnly))
    return;
  file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
  if (file.write(data) == data.size() && file.commit())
    lastPublished_ = data;
}
void WindowSwitcher::captureBackground(const QProcessEnvironment &environment) {
  if (!captureDirectory_.isValid()) {
    ready_ = true;
    publish();
    return;
  }
  // Keep one private snapshot. Closing a session never waits for a capture.
  const QString path =
      captureDirectory_.filePath(QString::number(serial_) + ".png");
  for (const auto &file :
       captureDirectory_.isValid()
           ? QDir(captureDirectory_.path()).entryList({"*.png"}, QDir::Files)
           : QStringList{})
    QFile::remove(captureDirectory_.filePath(file));
  auto *process = new QProcess(this);
  capture_ = process;
  process->setProcessEnvironment(environment);
  const auto complete = [this, process, path](bool success) {
    if (capture_ == process) {
      capture_ = nullptr;
      if (active_) {
        if (success && QFileInfo(path).size() > 0)
          background_ = QUrl::fromLocalFile(path).toString();
        ready_ = true;
        publish();
      }
    }
    process->deleteLater();
  };
  connect(process, &QProcess::finished, this,
          [complete](int code, QProcess::ExitStatus status) {
            complete(code == 0 && status == QProcess::NormalExit);
          });
  connect(process, &QProcess::errorOccurred, this,
          [complete](QProcess::ProcessError error) {
            if (error == QProcess::FailedToStart)
              complete(false);
          });
  QTimer::singleShot(350, process, [this, process] {
    if (capture_ != process)
      return;
    capture_ = nullptr;
    process->kill();
    ready_ = active_;
    publish();
  });
  process->start("grim", {"-s", "0.5", path});
}
} // namespace LunaDash
