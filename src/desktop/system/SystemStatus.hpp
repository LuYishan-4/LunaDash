#pragma once
#include "desktop/system/SystemMetrics.h"
#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QProcess>
#include <QString>

namespace LunaDash {
class SystemStatus final : public QObject {
public:
  explicit SystemStatus(QObject *parent = nullptr);
  QJsonObject snapshot() const;

private:
  void refresh();
  void refreshGpu();
  void finishGpuQuery(int code, QProcess::ExitStatus status);

  QJsonObject data_;
  QJsonArray history_;
  QJsonArray gpuHistory_;
  LuDashCpuCounters previous_{};
  QProcess *gpuQuery_ = nullptr;
  QString nvidiaSmi_;
  int refreshCount_ = 0;
};
} // namespace LunaDash
