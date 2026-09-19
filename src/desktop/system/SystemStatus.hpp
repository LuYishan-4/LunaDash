#pragma once
#include "desktop/system/SystemMetrics.h"
#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
namespace LunaDash {
class SystemStatus final : public QObject {
public:
  explicit SystemStatus(QObject *parent = nullptr);
  QJsonObject snapshot() const;

private:
  void refresh();
  QJsonObject data_;
  QJsonArray history_;
  LuDashCpuCounters previous_{};
};
} // namespace LunaDash
