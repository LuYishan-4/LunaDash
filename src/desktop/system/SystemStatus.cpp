#include "desktop/system/SystemStatus.hpp"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStorageInfo>
#include <QSysInfo>
#include <QTimer>
#include <QUrl>
#include <algorithm>

namespace LunaDash {
namespace {
QByteArray readStatusFile(const QString &path) {
  QFile file(path);
  return file.open(QIODevice::ReadOnly) ? file.read(128 * 1024).trimmed()
                                        : QByteArray{};
}

QString displayNameForUser(const QString &user) {
  QFile passwd(QStringLiteral("/etc/passwd"));
  if (!passwd.open(QIODevice::ReadOnly))
    return user;
  while (!passwd.atEnd()) {
    const auto fields = passwd.readLine().trimmed().split(':');
    if (fields.size() >= 5 && QString::fromUtf8(fields[0]) == user) {
      const QString gecos =
          QString::fromUtf8(fields[4]).section(',', 0, 0).trimmed();
      return gecos.isEmpty() ? user : gecos;
    }
  }
  return user;
}

QString avatarForUser(const QString &user) {
  const QString home = QDir::homePath();
  const QStringList candidates{
      home + QStringLiteral("/.face.icon"), home + QStringLiteral("/.face"),
      QStringLiteral("/var/lib/AccountsService/icons/") + user};
  for (const auto &path : candidates) {
    const QFileInfo info(path);
    if (info.isFile() && info.isReadable())
      return QUrl::fromLocalFile(info.canonicalFilePath()).toString();
  }
  return {};
}
} // namespace

SystemStatus::SystemStatus(QObject *parent) : QObject(parent) {
  const QString user = qEnvironmentVariable("USER");
  data_ = {{"os", QSysInfo::prettyProductName()},
           {"kernel", QSysInfo::kernelVersion()},
           {"architecture", QSysInfo::currentCpuArchitecture()},
           {"host", QSysInfo::machineHostName()},
           {"user", user},
           {"displayName", displayNameForUser(user)},
           {"avatar", avatarForUser(user)}};
  for (const auto &line : readStatusFile("/proc/cpuinfo").split('\n')) {
    if (line.startsWith("model name") || line.startsWith("Hardware")) {
      data_["cpuModel"] =
          QString::fromUtf8(line.mid(line.indexOf(':') + 1).trimmed());
      break;
    }
  }
  refresh();
  auto *timer = new QTimer(this);
  connect(timer, &QTimer::timeout, this, &SystemStatus::refresh);
  timer->start(1500);
}

QJsonObject SystemStatus::snapshot() const { return data_; }

void SystemStatus::refresh() {
  const auto cpuText = readStatusFile("/proc/stat");
  LuDashCpuCounters counters{};
  int cpu = 0;
  if (ludash_parse_cpu(cpuText.constData(), static_cast<size_t>(cpuText.size()),
                       &counters)) {
    cpu = ludash_cpu_percent(previous_, counters);
    previous_ = counters;
  }
  history_.append(cpu);
  if (history_.size() > 20)
    history_.removeFirst();
  data_["cpuPercent"] = cpu;
  data_["cpuHistory"] = history_;
  const auto memoryText = readStatusFile("/proc/meminfo");
  LuDashMemoryCounters memory{};
  ludash_parse_memory(memoryText.constData(),
                      static_cast<size_t>(memoryText.size()), &memory);
  data_["memoryUsed"] =
      static_cast<double>(ludash_memory_used(memory)) / (1024.0 * 1024.0);
  data_["memoryTotal"] =
      static_cast<double>(memory.total_kib) / (1024.0 * 1024.0);
  data_["memoryPercent"] = ludash_memory_percent(memory);
  const QStorageInfo storage(QDir::homePath());
  data_["diskUsed"] =
      static_cast<double>(storage.bytesTotal() - storage.bytesAvailable()) /
      (1024.0 * 1024.0 * 1024.0);
  data_["diskTotal"] =
      static_cast<double>(storage.bytesTotal()) / (1024.0 * 1024.0 * 1024.0);
  int battery = -1;
  for (const auto &entry : QDir("/sys/class/power_supply")
                               .entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
    const auto base = "/sys/class/power_supply/" + entry + "/";
    if (readStatusFile(base + "type") != "Battery")
      continue;
    bool valid = false;
    const int capacity = readStatusFile(base + "capacity").toInt(&valid);
    if (valid) {
      battery = std::clamp(capacity, 0, 100);
      break;
    }
  }
  data_["batteryPercent"] = battery;
}
} // namespace LunaDash
