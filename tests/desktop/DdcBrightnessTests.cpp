#include "DdcBrightnessTests.hpp"
#include "desktop/display/DdcBrightnessSettings.hpp"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QTemporaryDir>
#include <QThread>

namespace LunaDash {
namespace {
void require(bool condition, const char *message) {
  if (!condition)
    qFatal("%s", message);
}
void write(const QString &path, const QByteArray &data) {
  QFile file(path);
  require(file.open(QIODevice::WriteOnly), "Create DDC fixture");
  file.write(data);
}
QByteArray read(const QString &path) {
  QFile file(path);
  require(file.open(QIODevice::ReadOnly), "Read DDC fixture");
  return file.readAll();
}
void until(const std::function<bool()> &done) {
  QElapsedTimer elapsed;
  elapsed.start();
  while (!done() && elapsed.elapsed() < 6000) {
    QCoreApplication::processEvents();
    QThread::msleep(1);
  }
  require(done(), "DDC operation completed before test deadline");
}
} // namespace

void testDdcBrightness() {
  require(parseDdcBrightness("VCP 10 C 80 200\n").value("percent").toInt() ==
              40,
          "Normalize non-100 monitor maximum");
  for (const auto &invalid : {"VCP 10 ERR", "VCP 10 C 20 0", "VCP 10 C 101 100",
                              "VCP 12 C 50 100", "VCP 10 C 5 9999999999"})
    require(!parseDdcBrightness(invalid).value("available").toBool(),
            "Reject unsupported or invalid VCP values");
  require(parseDdcBrightness("VCP 10 C 0 100").value("available").toBool(),
          "Accept zero brightness");
  const QByteArray detected =
      "Display 1\n I2C bus: /dev/i2c-7\n Monitor: DEL:Panel A:123\n"
      "Invalid display\n I2C bus: /dev/i2c-2\n Monitor: BAD:Invalid:x\n"
      "Display 2\n I2C bus: /dev/i2c-8\n Monitor: ACR:Unsupported:456\n"
      "Display 3\n I2C bus: /dev/i2c-9\n Monitor: DEL:Panel B:789\n";
  require(parseDdcDisplays(detected).size() == 3,
          "Skip invalid display blocks");
  require(parseDdcDisplays(detected + detected).size() == 3,
          "Deduplicate I2C buses");
  QTemporaryDir directory;
  require(directory.isValid(), "Private DDC fixture directory");
  write(directory.filePath("displays"), detected);
  write(directory.filePath("7"), "80");
  write(directory.filePath("9"), "50");
  write(directory.filePath("ddcutil"), R"SH(#!/bin/sh
set -eu
root="$LUNADASH_DDC_FIXTURE"
if [ "$1" = detect ]; then
  /bin/cat "$root/displays"
  exit 0
fi
operation="$1"
value="${3:-}"
bus=""
while [ "$#" -gt 0 ]; do
  if [ "$1" = --bus ]; then shift; bus="$1"; fi
  shift
done
case "$bus" in
  7) maximum=200;;
  9) maximum=100;;
  *) printf 'VCP 10 ERR\n'; exit 1;;
esac
if [ "$operation" = setvcp ]; then
  printf '%s %s\n' "$bus" "$value" >> "$root/writes"
  /bin/sleep 0.15
  if [ -e "$root/fail" ]; then exit 1; fi
  printf '%s' "$value" > "$root/$bus"
else
  printf 'VCP 10 C %s %s\n' "$(/bin/cat "$root/$bus")" "$maximum"
fi
)SH");
  QFile::setPermissions(directory.filePath("ddcutil"),
                        QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
  const auto oldPath = qgetenv("PATH");
  const auto oldFixture = qgetenv("LUNADASH_DDC_FIXTURE");
  qputenv("PATH", directory.path().toUtf8());
  qputenv("LUNADASH_DDC_FIXTURE", directory.path().toUtf8());
  DdcBrightnessSettings settings;
  until([&] {
    return settings.snapshot().value("devices").toArray().size() == 3 &&
           !settings.snapshot().value("scanning").toBool();
  });
  auto devices = settings.snapshot().value("devices").toArray();
  const auto firstId = devices[0].toObject().value("id").toString();
  const auto lastId = devices[2].toObject().value("id").toString();
  require(devices[0].toObject().value("percent").toInt() == 40,
          "Read monitor brightness");
  require(!devices[1].toObject().value("available").toBool(),
          "Disable unsupported monitor");
  QString error;
  require(!settings.setPercent("unknown", 50, &error),
          "Reject unknown monitor");
  require(!settings.setPercent(firstId, 101, &error), "Reject invalid percent");
  require(!settings.setPercent(devices[1].toObject().value("id").toString(), 50,
                               &error),
          "Reject unsupported monitor writes");
  QElapsedTimer elapsed;
  elapsed.start();
  require(settings.setPercent(firstId, 60, &error), "Start asynchronous write");
  require(elapsed.elapsed() < 100, "DDC write must not block the event loop");
  for (int value : {61, 62, 63, 64, 65})
    require(settings.setPercent(firstId, value, &error),
            "Queue latest brightness");
  require(settings.setPercent(lastId, 25, &error),
          "Target second monitor separately");
  settings
      .refresh(); // A refresh during writes must not invalidate their targets.
  until([&] {
    return !settings.snapshot().value("busy").toBool() &&
           !settings.snapshot().value("scanning").toBool();
  });
  require(read(directory.filePath("writes")) == "7 120\n7 130\n9 25\n",
          "Coalesce requests and use each monitor's bus and maximum");
  devices = settings.snapshot().value("devices").toArray();
  require(devices[0].toObject().value("percent").toInt() == 65,
          "Publish verified percentage");
  write(directory.filePath("fail"), "1");
  require(settings.setPercent(firstId, 80, &error), "Start failing write");
  until([&] { return !settings.snapshot().value("busy").toBool(); });
  const auto failed =
      settings.snapshot().value("devices").toArray()[0].toObject();
  require(failed.value("percent").toInt() == 65 &&
              !failed.value("error").toString().isEmpty(),
          "Keep last confirmed value on write failure");
  write(directory.filePath("displays"), "");
  settings.refresh();
  until([&] { return !settings.snapshot().value("scanning").toBool(); });
  require(settings.snapshot().value("devices").toArray().isEmpty(),
          "Remove unplugged monitors");
  require(!settings.setPercent(firstId, 50, &error), "Reject stale device ID");
  QFile::remove(directory.filePath("ddcutil"));
  settings.refresh();
  require(!settings.snapshot().value("installed").toBool(),
          "Report missing helper without hardware fallback");
  qputenv("PATH", oldPath);
  if (oldFixture.isNull())
    qunsetenv("LUNADASH_DDC_FIXTURE");
  else
    qputenv("LUNADASH_DDC_FIXTURE", oldFixture);

  CommandRunner bounded;
  bool finished = false, timedOut = false;
  bounded.run(
      "/bin/sh", {"-c", "exec /bin/sleep 3"},
      [&](bool ok, const QByteArray &) {
        finished = true;
        timedOut = !ok;
      },
      50);
  until([&] { return finished; });
  require(timedOut, "Command timeout reports failure and releases the queue");
}
} // namespace LunaDash
