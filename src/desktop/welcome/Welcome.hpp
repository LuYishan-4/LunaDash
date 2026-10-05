#pragma once
#include <QObject>
#include <QQueue>
#include <QVariantMap>

class QWidget;

namespace LunaDash {
class WelcomeBridge final : public QObject {
  Q_OBJECT
  Q_PROPERTY(QVariantMap state READ state NOTIFY stateChanged)
public:
  explicit WelcomeBridge(QObject *parent);
  QVariantMap state() const;
  Q_INVOKABLE void command(const QString &method, const QString &value);
  Q_INVOKABLE void finish();
signals:
  void stateChanged();
  void finished();
private:
  void dispatch();
  void reportError();
  QVariantMap state_;
  QQueue<QPair<QString, QString>> commands_;
  bool pending_ = false;
};
QWidget *createWelcome();
} // namespace LunaDash
