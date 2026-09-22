#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QProcess>
#include <QTemporaryDir>
#include <QTimer>

namespace LunaDash {
// Owns one selection session and its private, event-driven shell feedback file.
class WindowSwitcher final : public QObject {
public:
  explicit WindowSwitcher(QObject *parent = nullptr);
  ~WindowSwitcher() override;
  void setChannelPath(const QString &path);
  bool begin(const QJsonArray &windows, int focused, int direction,
             const QProcessEnvironment &environment);
  bool active() const;
  void step(int direction);
  bool select(int window);
  int finish(bool accept);
  void remove(int window);
  void setDrag(const QJsonObject &drag);
  QJsonObject snapshot() const;
  int serial() const;
  QString thumbnailPath(int window) const;
  void setThumbnail(int serial, int window, const QString &path);
  void setLayout(const QJsonArray &clients, int workspace, bool dragging);

private:
  QString channelPath_;
  QByteArray lastPublished_;
  QJsonArray workspaces_;
  QJsonObject drag_;
  QJsonArray clients_;
  int workspace_ = 0;
  bool dragging_ = false;
  int launcherSerial_ = 0;
  QTimer feedbackTimer_;
  int index_ = 0;
  int serial_ = 0;
  bool active_ = false;
  bool ready_ = false;
  QString background_;
  QTemporaryDir captureDirectory_;
  QProcess *capture_ = nullptr;
  void publish();
  void writeSnapshot();
  void captureBackground(const QProcessEnvironment &environment);
};
} // namespace LunaDash
