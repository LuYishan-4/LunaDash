#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QProcess>
#include <QTemporaryDir>

namespace LunaDash {
// Owns one selection session and its private, event-driven shell feedback file.
class WindowSwitcher final : public QObject {
public:
  explicit WindowSwitcher(QObject *parent = nullptr);
  ~WindowSwitcher() override;
  void setChannelPath(const QString &path);
  void recordFocus(int window);
  bool begin(const QJsonArray &windows, int focused, int direction,
             const QProcessEnvironment &environment);
  bool active() const;
  void step(int direction);
  bool select(int window);
  int finish(bool accept);
  void remove(int window);
  void setDrag(const QJsonObject &drag);
  QJsonObject snapshot() const;

private:
  QString channelPath_;
  QByteArray lastPublished_;
  QList<int> history_;
  QJsonArray windows_;
  QJsonObject drag_;
  int index_ = 0;
  int serial_ = 0;
  bool active_ = false;
  bool ready_ = false;
  QString background_;
  QTemporaryDir captureDirectory_;
  QProcess *capture_ = nullptr;
  void publish();
  void captureBackground(const QProcessEnvironment &environment);
};
} // namespace LunaDash
