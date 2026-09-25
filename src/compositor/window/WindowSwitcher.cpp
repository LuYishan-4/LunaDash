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
WindowSwitcher::WindowSwitcher(QObject *parent) : QObject(parent) {
  feedbackTimer_.setSingleShot(true);
  feedbackTimer_.setInterval(16);
  connect(&feedbackTimer_, &QTimer::timeout, this,
          &WindowSwitcher::writeSnapshot);
}
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
  writeSnapshot();
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
  workspaces_ = windows;
  index_ = 0;
  for (int i = 0; i < workspaces_.size(); ++i)
    if (workspaces_[i].toObject().value("id").toInt() == focused)
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
  if (!active_ || workspaces_.isEmpty())
    return;
  const int count = static_cast<int>(workspaces_.size());
  index_ = ((index_ + direction) % count + count) % count;
  publish();
}
bool WindowSwitcher::select(int window) {
  if (!active_)
    return false;
  for (int i = 0; i < workspaces_.size(); ++i) {
    if (workspaces_[i].toObject().value("id").toInt() != window)
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
  const int selected = accept && !workspaces_.isEmpty()
                           ? workspaces_[index_].toObject().value("id").toInt()
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
  for (int w = 0; w < workspaces_.size(); ++w) {
    auto workspace = workspaces_[w].toObject();
    auto members = workspace.value("windows").toArray();
    for (int i = static_cast<int>(members.size()); i-- > 0;)
      if (members[i].toObject().value("id").toInt() == window)
        members.removeAt(i);
    workspace["windows"] = members;
    workspaces_[w] = workspace;
  }
  publish();
}
int WindowSwitcher::serial() const { return serial_; }
QString WindowSwitcher::thumbnailPath(int window) const {
  return captureDirectory_.filePath(
      QString("%1-window-%2.png").arg(serial_).arg(window));
}
void WindowSwitcher::setThumbnail(int serial, int window, const QString &path) {
  if (!active_ || serial != serial_) {
    QFile::remove(path);
    return;
  }
  for (int w = 0; w < workspaces_.size(); ++w) {
    auto workspace = workspaces_[w].toObject();
    auto members = workspace.value("windows").toArray();
    for (int i = 0; i < members.size(); ++i) {
      auto member = members[i].toObject();
      if (member.value("id").toInt() != window)
        continue;
      member["thumbnail"] = QUrl::fromLocalFile(path).toString();
      members[i] = member;
    }
    workspace["windows"] = members;
    workspaces_[w] = workspace;
  }
  publish();
}
void WindowSwitcher::setLayout(const QJsonArray &clients, int workspace,
                               bool dragging) {
  clients_ = clients;
  workspace_ = workspace;
  dragging_ = dragging;
  publish();
}
void WindowSwitcher::setLauncherState(int serial, bool open) {
  if (launcherSerial_ == serial && launcherOpen_ == open)
    return;
  launcherSerial_ = serial;
  launcherOpen_ = open;
  publish();
}
void WindowSwitcher::setDrag(const QJsonObject &drag) {
  drag_ = drag;
  publish();
}
QJsonObject WindowSwitcher::snapshot() const {
  return {{"active", active_},
          {"ready", ready_},
          {"index", index_},
          {"serial", serial_},
          {"workspaces", workspaces_},
          {"background", background_},
          {"drag", drag_},
          {"clients", clients_},
          {"workspace", workspace_},
          {"dragging", dragging_},
          {"launcherSerial", launcherSerial_},
          {"launcherOpen", launcherOpen_}};
}
void WindowSwitcher::publish() {
  if (!feedbackTimer_.isActive())
    feedbackTimer_.start();
}
void WindowSwitcher::writeSnapshot() {
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
