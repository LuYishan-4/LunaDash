#include "shell/runtime/ShellRenderer.hpp"
#include <QtTest>

namespace LunaDash {
class ShellRenderingTests final : public QObject {
  Q_OBJECT
private slots:
  void explicitVulkan() {
    QProcessEnvironment environment;
    environment.insert("LUDASH_SHELL_RENDERER", "vulkan");
    environment.insert("QT_QUICK_BACKEND", "software");
    QVERIFY(configureShellRendering(environment, true));
    QCOMPARE(environment.value("QSG_RHI_BACKEND"), "vulkan");
    QVERIFY(!environment.contains("QT_QUICK_BACKEND"));
  }
  void automaticSelection() {
    QProcessEnvironment environment;
    QVERIFY(configureShellRendering(environment, true));
    QCOMPARE(environment.value("QT_QUICK_BACKEND"), "software");
    environment = QProcessEnvironment();
    QVERIFY(configureShellRendering(environment, false));
    QCOMPARE(environment.value("QSG_RHI_BACKEND"), "opengl");
  }
  void preserveToolkitChoice() {
    QProcessEnvironment environment;
    environment.insert("QSG_RHI_BACKEND", "vulkan");
    QVERIFY(configureShellRendering(environment, true));
    QCOMPARE(environment.value("QSG_RHI_BACKEND"), "vulkan");
  }
  void invalidChoiceDoesNotMutateEnvironment() {
    QProcessEnvironment environment;
    environment.insert("LUDASH_SHELL_RENDERER", "typo");
    const auto original = environment;
    QVERIFY(!configureShellRendering(environment, false));
    QCOMPARE(environment, original);
  }
};
} // namespace LunaDash
QTEST_GUILESS_MAIN(LunaDash::ShellRenderingTests)
#include "ShellRenderingTests.moc"
