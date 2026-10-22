#pragma once

#include <QJsonObject>
#include <QObject>
#include <QString>

namespace LunaDash {

bool isValidSessionAction(const QString &action);

class SessionActions final : public QObject {
public:
  explicit SessionActions(QObject *parent = nullptr);

  QJsonObject snapshot() const;
  bool execute(const QString &action, QString *error = nullptr);

private:
  void refreshAvailability();

  QJsonObject availability_;
  QString availabilityError_;
};

} // namespace LunaDash
