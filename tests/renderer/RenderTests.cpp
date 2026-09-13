#include <LuDash/renderer/RenderBackend.h>
#include <QGuiApplication>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>
#include <QOpenGLExtraFunctions>
#include <QOpenGLShaderProgram>
#include <LuDash/render_core/ShaderProgram.h>
#include <LuDash/render_core/BlurPass.h>
#include <memory>
#include <QtTest>
namespace LuDash {
class RenderTests : public QObject {
    Q_OBJECT
private slots:
    void configuredContextSupportsQtMaterials() {
        QVERIFY(QCoreApplication::testAttribute(Qt::AA_ShareOpenGLContexts));
        auto* shared = QOpenGLContext::globalShareContext();
        QVERIFY(shared);
        QVERIFY(shared->isValid());
        const auto format = QSurfaceFormat::defaultFormat();
        if (format.renderableType() == QSurfaceFormat::OpenGLES)
            QCOMPARE(format.profile(), QSurfaceFormat::NoProfile);
        else {
            QCOMPARE(format.profile(), QSurfaceFormat::CompatibilityProfile);
            QVERIFY(format.testOption(QSurfaceFormat::DeprecatedFunctions));
        }
    }
    void shaderContexts_data() {
        QTest::addColumn<bool>("gles");
        QTest::addColumn<bool>("compatibility");
        QTest::newRow("OpenGL-3.3-Core") << false << false;
        QTest::newRow("OpenGL-3.3-Compatibility") << false << true;
        QTest::newRow("OpenGL-ES-3.0") << true << false;
    }
    void shaderContexts() {
        QFETCH(bool, gles);
        QFETCH(bool, compatibility);
        QSurfaceFormat format; format.setRenderableType(gles ? QSurfaceFormat::OpenGLES : QSurfaceFormat::OpenGL);
        format.setVersion(3, gles ? 0 : 3);
        format.setProfile(gles ? QSurfaceFormat::NoProfile : compatibility ? QSurfaceFormat::CompatibilityProfile : QSurfaceFormat::CoreProfile);
        if (compatibility) format.setOption(QSurfaceFormat::DeprecatedFunctions);
        QOpenGLContext context; context.setFormat(format); QVERIFY(context.create()); QCOMPARE(context.isOpenGLES(), gles);
        QOffscreenSurface surface; surface.setFormat(context.format()); surface.create(); QVERIFY(surface.isValid());
        QVERIFY(context.makeCurrent(&surface));
        if (compatibility) {
            QCOMPARE(context.format().profile(), QSurfaceFormat::CompatibilityProfile);
            // The Qt external-texture material's desktop variant needs GLSL 120.
            QOpenGLShaderProgram legacy;
            QVERIFY2(legacy.addShaderFromSourceCode(QOpenGLShader::Vertex,
                "#version 120\nattribute vec4 position; void main() { gl_Position = position; }"), qPrintable(legacy.log()));
            QVERIFY2(legacy.addShaderFromSourceCode(QOpenGLShader::Fragment,
                "#version 120\nvoid main() { gl_FragColor = vec4(1.0); }"), qPrintable(legacy.log()));
            QVERIFY2(legacy.link(), qPrintable(legacy.log()));
        }
        char error[1024]{};
        const auto vertex = shaderSource("wallpaper.vert", gles), fragment = shaderSource("wallpaper.frag", gles);
        auto* wallpaper = ludash_shader_create(resolveGLFunction, vertex.constData(), fragment.constData(), error, sizeof(error));
        QVERIFY2(wallpaper, error);
        QOpenGLFramebufferObject target(QSize(64, 64)); QVERIFY(target.isValid()); QVERIFY(target.bind());
        QVERIFY(!ludash_wallpaper_draw(wallpaper, -1, 64, 0));
        QVERIFY(ludash_wallpaper_draw(wallpaper, 64, 64, 0));
        const auto before = target.toImage(); QVERIFY(!before.isNull());
        const auto blurVertex = shaderSource("blur/blur.vert", gles), blurFragment = shaderSource("blur/blur.frag", gles);
        auto* blur = ludash_blur_create(resolveGLFunction, blurVertex.constData(), blurFragment.constData(), error, sizeof(error));
        QVERIFY2(blur, error);
        QVERIFY(target.bind()); context.extraFunctions()->glViewport(0, 0, 64, 64);
        LuDashBlurRegion region{0, 0, 64, 64, 18, 1, 0, {0, 0, 64, 64}, 0, 0};
        QVERIFY(ludash_blur_draw(blur, &region));
        QCOMPARE(context.extraFunctions()->glGetError(), GLenum(GL_NO_ERROR));
        const auto after = target.toImage(); QVERIFY(!after.isNull()); QVERIFY(before != after);
        // A bright vertical strip must soften into a continuous, monotonic falloff.
        QVERIFY(target.bind());
        auto* gl = context.extraFunctions();
        gl->glDisable(GL_SCISSOR_TEST); gl->glClearColor(0, 0, 0, 1); gl->glClear(GL_COLOR_BUFFER_BIT);
        gl->glEnable(GL_SCISSOR_TEST); gl->glScissor(28, 0, 8, 64);
        gl->glClearColor(1, 1, 1, 1); gl->glClear(GL_COLOR_BUFFER_BIT); gl->glDisable(GL_SCISSOR_TEST);
        QVERIFY(ludash_blur_draw(blur, &region));
        const auto softened = target.toImage();
        QVERIFY(qRed(softened.pixel(32, 32)) > qRed(softened.pixel(42, 32)));
        for (int x = 33; x < 54; ++x) {
            const int previous = qRed(softened.pixel(x - 1, 32)), current = qRed(softened.pixel(x, 32));
            QVERIFY2(current <= previous + 1 && previous - current <= 24, "Blur contains repeated bands or abrupt sampling gaps");
        }
        // Transposing the input must preserve the same smooth kernel in Y.
        QVERIFY(target.bind());
        gl->glClearColor(0, 0, 0, 1); gl->glClear(GL_COLOR_BUFFER_BIT);
        gl->glEnable(GL_SCISSOR_TEST); gl->glScissor(0, 28, 64, 8);
        gl->glClearColor(1, 1, 1, 1); gl->glClear(GL_COLOR_BUFFER_BIT); gl->glDisable(GL_SCISSOR_TEST);
        QVERIFY(ludash_blur_draw(blur, &region));
        const auto vertical = target.toImage();
        for (int coordinate = 8; coordinate < 56; ++coordinate)
            QVERIFY2(qAbs(qRed(vertical.pixel(32, coordinate)) - qRed(softened.pixel(coordinate, 32))) <= 2,
                     "Horizontal and vertical Gaussian kernels differ");
        // Normalization must preserve a constant image, including odd/max radii.
        for (const int radius : {0, 1, 18, 31, 32}) {
            QVERIFY(target.bind()); gl->glClearColor(.25f, .5f, .75f, 1); gl->glClear(GL_COLOR_BUFFER_BIT);
            region.radius = radius;
            QVERIFY(ludash_blur_draw(blur, &region));
            const auto constant = target.toImage();
            for (const QPoint point : {QPoint(0, 0), QPoint(32, 32), QPoint(63, 63)}) {
                const auto pixel = constant.pixel(point);
                QVERIFY(qAbs(qRed(pixel) - 64) <= 1);
                QVERIFY(qAbs(qGreen(pixel) - 128) <= 1);
                QVERIFY(qAbs(qBlue(pixel) - 191) <= 1);
            }
        }
        region.radius = 100; QVERIFY(!ludash_blur_draw(blur, &region));
        ludash_blur_destroy(blur); ludash_shader_destroy(wallpaper);
        // FBO destruction below still has the correct context current.

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
