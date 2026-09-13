#include <LuDash/renderer/RenderBackend.h>
#include <QGuiApplication>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLShaderProgram>
#include <QtTest>
namespace LuDash {
class RenderTests : public QObject {
    Q_OBJECT
private slots:
    void shaderContexts_data() {
        QTest::addColumn<bool>("gles");
        QTest::newRow("OpenGL-3.3") << false;
        QTest::newRow("OpenGL-ES-3.0") << true;
    }
    void shaderContexts() {
        QFETCH(bool, gles);
        QSurfaceFormat format; format.setRenderableType(gles ? QSurfaceFormat::OpenGLES : QSurfaceFormat::OpenGL);
        format.setVersion(3, gles ? 0 : 3); format.setProfile(gles ? QSurfaceFormat::NoProfile : QSurfaceFormat::CoreProfile);
        QOpenGLContext context; context.setFormat(format); QVERIFY(context.create()); QCOMPARE(context.isOpenGLES(), gles);
        QOffscreenSurface surface; surface.setFormat(context.format()); surface.create(); QVERIFY(surface.isValid());
        QVERIFY(context.makeCurrent(&surface));
        QOpenGLShaderProgram program;
        QVERIFY2(program.addShaderFromSourceCode(QOpenGLShader::Vertex, shaderSource("wallpaper.vert", gles)), qPrintable(program.log()));
        QVERIFY2(program.addShaderFromSourceCode(QOpenGLShader::Fragment, shaderSource("wallpaper.frag", gles)), qPrintable(program.log()));
        QVERIFY2(program.link(), qPrintable(program.log()));
        program.removeAllShaders(); context.doneCurrent();
    }
};
}
int main(int argc, char** argv) {
    LuDash::configureGraphics(LuDash::GraphicsApi::Auto);
    QGuiApplication application(argc, argv);
    LuDash::RenderTests tests;
    return QTest::qExec(&tests, argc, argv);
}
#include "RenderTests.moc"
