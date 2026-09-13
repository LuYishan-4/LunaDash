#include <LuDash/shell_renderer/ShellRenderer.h>
#include <QtTest>
namespace LuDash {
class ShellRendererTests : public QObject {
    Q_OBJECT
private slots:
    void backendSelection_data() {
        QTest::addColumn<QString>("requested");
        QTest::addColumn<bool>("nvidia");
        QTest::addColumn<QString>("expected");
        QTest::newRow("NVIDIA-auto") << QString("auto") << true << QString("software");
        QTest::newRow("other-auto") << QString("auto") << false << QString("opengl");
        QTest::newRow("explicit-GPU") << QString("opengl") << true << QString("opengl");
        QTest::newRow("explicit-software") << QString("software") << false << QString("software");
    }
    void backendSelection() {
        QFETCH(QString, requested); QFETCH(bool, nvidia); QFETCH(QString, expected);
        QProcessEnvironment env; env.insert("LUDASH_SHELL_RENDERER", requested);
        env.insert("QT_QUICK_BACKEND", "old"); env.insert("QSG_RHI_BACKEND", "old");
        env.insert("WAYLAND_DISPLAY", "test-display");
        QVERIFY(configureShellRendering(env, nvidia));
        QCOMPARE(env.value("LUDASH_SHELL_RENDERER"), expected);
        QCOMPARE(env.value("WAYLAND_DISPLAY"), QString("test-display"));
        if (expected == "software") {
            QCOMPARE(env.value("QT_QUICK_BACKEND"), QString("software"));
            QVERIFY(!env.contains("QSG_RHI_BACKEND"));
        } else {
            QCOMPARE(env.value("QSG_RHI_BACKEND"), QString("opengl"));
            QVERIFY(!env.contains("QT_QUICK_BACKEND"));
        }
    }
    void invalidBackendPreservesEnvironment() {
        QProcessEnvironment env; env.insert("LUDASH_SHELL_RENDERER", "invalid");
        const auto before = env;
        QVERIFY(!configureShellRendering(env, true)); QCOMPARE(env, before);
    }
};
}
QTEST_APPLESS_MAIN(LuDash::ShellRendererTests)
#include "ShellRendererTests.moc"
