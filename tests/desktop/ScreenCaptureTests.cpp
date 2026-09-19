#include "compositor/capture/ScreenCapture.hpp"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QTemporaryDir>
#include <QThread>
#include <cstdio>

namespace LunaDash {
namespace {
void check(bool condition, const char *message) {
  if (!condition)
    qFatal("%s", message);
}

void writeFile(const QString &path, const QByteArray &contents) {
  QFile file(path);
  check(file.open(QIODevice::WriteOnly), "Open capture fixture");
  check(file.write(contents) == contents.size(), "Write capture fixture");
  check(file.setPermissions(QFile::ReadOwner | QFile::WriteOwner |
                            QFile::ExeOwner),
        "Set fixture permissions");
}

void runTests() {
  QTemporaryDir directory;
  check(directory.isValid(), "Create isolated capture directory");
  const auto oldPath = qgetenv("PATH");
  qputenv("PATH", directory.path().toUtf8() + ':' + oldPath);
  // Match the real selector's startup contract: read candidates to EOF before
  // opening its overlay. Leaving a managed input pipe open hangs this fixture.
  writeFile(directory.filePath("slurp"),
            "#!/bin/sh\ncat >/dev/null\nsleep 0.1\n"
            "case \"$(cat \"$CAPTURE_TEST_MODE\")\" in\n"
            "cancel) echo 'selection cancelled' >&2; exit 1;;\n"
            "error) echo 'failed to create display' >&2; exit 1;;\n"
            "*) echo '10,20 160x90';;\nesac\n");
  writeFile(directory.filePath("grim"),
            "#!/bin/sh\ncat >/dev/null\n"
            "test \"$1\" = '-g' && test \"$2\" = '10,20 160x90' || exit 2\n"
            "printf 'region fixture' >\"$3\"\n");
  auto environment = QProcessEnvironment::systemEnvironment();
  const auto mode = directory.filePath("mode");
  environment.insert("CAPTURE_TEST_MODE", mode);
  ScreenCapture capture;
  bool done = false;
  QString saved, failure;
  QObject::connect(&capture, &ScreenCapture::completed,
                   [&](const QString &path, const QString &error) {
                     done = true;
                     saved = path;
                     failure = error;
                   });
  int iteration = 0;
  for (const auto &selection : {"select", "cancel", "error", "select"}) {
    writeFile(mode, selection);
    const auto path =
        directory.filePath(QString("shot-%1.png").arg(iteration++));
    done = false;
    QString error;
    check(capture.selectRegion(environment, path, &error), "Start selection");
    check(!capture.selectRegion(environment, path, &error),
          "Repeated requests cannot replace an active selection");
    check(capture.busy(), "Selection reports busy");
    QElapsedTimer elapsed;
    elapsed.start();
    while (!done && elapsed.elapsed() < 3000) {
      QCoreApplication::processEvents();
      QThread::msleep(1);
    }
    check(done && !capture.busy(), "Selector must reach EOF and finish");
    if (QByteArray(selection) == "select") {
      check(capture.phase() == "saved" && saved == path && failure.isEmpty(),
            "Selection succeeds after startup and after a failure");
      QFile file(path);
      check(file.open(QIODevice::ReadOnly) &&
                file.readAll() == "region fixture",
            "Capture helper receives only the selected geometry");
    } else {
      check(saved.isEmpty() && !QFile::exists(path),
            "Cancelled or failed captures create no image");
      if (QByteArray(selection) == "cancel")
        check(capture.phase() == "cancelled" && failure.isEmpty(),
              "The real slurp cancellation message is not an error");
      else
        check(capture.phase() == "failed" &&
                  failure.contains("failed to create display"),
              "Other selector failures retain their diagnostic");
    }
  }
  qputenv("PATH", oldPath);
  std::puts("Capture startup EOF, repeat protection, cancellation, diagnostics "
            "and retry passed.");
}
} // namespace
} // namespace LunaDash

int main(int argc, char **argv) {
  QCoreApplication application(argc, argv);
  LunaDash::runTests();
  return 0;
}
