#include "desktop/system/SystemStatus.hpp"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
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

QString driverForCard(const QString &card) {
  const QFileInfo driver(card + QStringLiteral("/device/driver"));
  if (!driver.exists())
    return {};
  const QString target = driver.symLinkTarget();
  return target.isEmpty() ? QString() : QFileInfo(target).fileName();
}

QString friendlyCardName(const QString &card) {
  for (const auto &name : {QStringLiteral("product_name"),
                           QStringLiteral("model"),
                           QStringLiteral("name")}) {
    const auto value = readStatusFile(card + QStringLiteral("/device/") + name);
    if (!value.isEmpty())
      return QString::fromUtf8(value);
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
           {"avatar", avatarForUser(user)},
           {"gpuAvailable", false},
           {"gpuPercent", -1}};
  for (const auto &line : readStatusFile("/proc/cpuinfo").split('\n')) {
    if (line.startsWith("model name") || line.startsWith("Hardware")) {
      data_["cpuModel"] =
          QString::fromUtf8(line.mid(line.indexOf(':') + 1).trimmed());
      break;
    }
  }

  nvidiaSmi_ = QStandardPaths::findExecutable(QStringLiteral("nvidia-smi"));
  gpuQuery_ = new QProcess(this);
  gpuQuery_->setProcessChannelMode(QProcess::SeparateChannels);
  connect(gpuQuery_, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
          this, [this](int code, QProcess::ExitStatus status) {
            finishGpuQuery(code, status);
          });

  refresh();
  auto *timer = new QTimer(this);
  connect(timer, &QTimer::timeout, this, &SystemStatus::refresh);
  timer->start(3000);
}

QJsonObject SystemStatus::snapshot() const { return data_; }

void SystemStatus::refreshGpu() {
  int busy = -1;
  QString model;
  QString driver;
  bool nvidia = false;

  const QDir drm(QStringLiteral("/sys/class/drm"));
  const auto cards = drm.entryList({QStringLiteral("card[0-9]*")},
                                   QDir::Dirs | QDir::NoDotAndDotDot,
                                   QDir::Name);
  for (const auto &entry : cards) {
    const QString card = drm.filePath(entry);
    const QString cardDriver = driverForCard(card);
    if (driver.isEmpty() && !cardDriver.isEmpty())
      driver = cardDriver;
    const auto vendor = readStatusFile(card + QStringLiteral("/device/vendor"));
    nvidia = nvidia || vendor == "0x10de" || cardDriver == "nvidia";

    bool valid = false;
    const int current =
        readStatusFile(card + QStringLiteral("/device/gpu_busy_percent"))
            .toInt(&valid);
    if (valid)
      busy = std::max(busy, std::clamp(current, 0, 100));
    if (model.isEmpty())
      model = friendlyCardName(card);
  }

  if (!driver.isEmpty())
    data_["gpuDriver"] = driver;
  if (!model.isEmpty())
    data_["gpuModel"] = model;

  if (busy >= 0) {
    data_["gpuAvailable"] = true;
    data_["gpuPercent"] = busy;
    gpuHistory_.append(busy);
    if (gpuHistory_.size() > 20)
      gpuHistory_.removeFirst();
    data_["gpuHistory"] = gpuHistory_;
    return;
  }

  if ((nvidia || driver == "nvidia") && !nvidiaSmi_.isEmpty()) {
    if (gpuQuery_ && gpuQuery_->state() == QProcess::NotRunning) {
      gpuQuery_->start(
          nvidiaSmi_,
          {QStringLiteral("--query-gpu=name,utilization.gpu"),
           QStringLiteral("--format=csv,noheader,nounits")});
    }
    return;
  }

  data_["gpuAvailable"] = false;
  data_["gpuPercent"] = -1;
}

void SystemStatus::finishGpuQuery(int code, QProcess::ExitStatus status) {
  if (!gpuQuery_ || status != QProcess::NormalExit || code != 0) {
    data_["gpuAvailable"] = false;
    data_["gpuPercent"] = -1;
    return;
  }

  int busy = -1;
  QStringList models;
  const auto lines = gpuQuery_->readAllStandardOutput().split('\n');
  for (const auto &raw : lines) {
    const auto line = raw.trimmed();
    if (line.isEmpty())
      continue;
    const int separator = line.lastIndexOf(',');
    if (separator <= 0)
      continue;
    bool valid = false;
    const int current = line.mid(separator + 1).trimmed().toInt(&valid);
    if (!valid)
      continue;
    const QString name = QString::fromUtf8(line.left(separator).trimmed());
    if (!name.isEmpty() && !models.contains(name))
      models.append(name);
    busy = std::max(busy, std::clamp(current, 0, 100));
  }

  if (busy < 0) {
    data_["gpuAvailable"] = false;
    data_["gpuPercent"] = -1;
    return;
  }

  data_["gpuAvailable"] = true;
  data_["gpuPercent"] = busy;
  data_["gpuDriver"] = QStringLiteral("nvidia");
  if (!models.isEmpty())
    data_["gpuModel"] = models.join(QStringLiteral(" / "));
  gpuHistory_.append(busy);
  if (gpuHistory_.size() > 20)
    gpuHistory_.removeFirst();
  data_["gpuHistory"] = gpuHistory_;
}

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

  const bool slowSample = (refreshCount_ % 3) == 0;
  const bool gpuSample = (refreshCount_ % 2) == 0;

  if (slowSample) {
    const QStorageInfo storage(QDir::homePath());
    const qint64 total = storage.bytesTotal();
    const qint64 available = storage.bytesAvailable();
    const qint64 used = total > available ? total - available : 0;
    data_["diskUsed"] =
        static_cast<double>(used) / (1024.0 * 1024.0 * 1024.0);
    data_["diskTotal"] =
        static_cast<double>(std::max<qint64>(0, total)) /
        (1024.0 * 1024.0 * 1024.0);
    data_["diskPercent"] =
        total > 0 ? static_cast<int>(100.0 * static_cast<double>(used) /
                                     static_cast<double>(total))
                  : 0;
    const QString diskDevice = QString::fromUtf8(storage.device());
    data_["diskDevice"] =
        diskDevice.isEmpty() ? storage.name() : diskDevice;

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

  if (gpuSample)
    refreshGpu();
  ++refreshCount_;
}
} // namespace LunaDash
