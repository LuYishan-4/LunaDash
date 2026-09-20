#include "compositor/renderer/capture/ThumbnailReadback.h"
#include <QByteArray>
#include <QtTest>
#include <drm_fourcc.h>
#define static
extern "C" {
#include <wlr/interfaces/wlr_buffer.h>
#include <wlr/render/allocator.h>
#include <wlr/render/drm_format_set.h>
#include <wlr/render/pixman.h>
#include <wlr/render/wlr_texture.h>
}
#undef static

namespace LunaDash {
namespace {
struct ImageBuffer {
  wlr_buffer base{};
  QByteArray pixels;
};
void destroyImage(wlr_buffer *buffer) {
  delete reinterpret_cast<ImageBuffer *>(buffer);
}
bool accessImage(wlr_buffer *buffer, uint32_t, void **data, uint32_t *format,
                 size_t *stride) {
  auto *image = reinterpret_cast<ImageBuffer *>(buffer);
  *data = image->pixels.data();
  *format = DRM_FORMAT_ARGB8888;
  *stride = buffer->width * 4;
  return true;
}
void finishAccessImage(wlr_buffer *) {}
const wlr_buffer_impl bufferImplementation{.destroy = destroyImage,
                                           .get_dmabuf = nullptr,
                                           .get_shm = nullptr,
                                           .begin_data_ptr_access = accessImage,
                                           .end_data_ptr_access =
                                               finishAccessImage};
wlr_buffer *allocateImage(wlr_allocator *, int width, int height,
                          const wlr_drm_format *) {
  auto *buffer = new ImageBuffer;
  buffer->pixels.resize(width * height * 4);
  wlr_buffer_init(&buffer->base, &bufferImplementation, width, height);
  return &buffer->base;
}
void destroyAllocator(wlr_allocator *allocator) { delete allocator; }
const wlr_allocator_interface allocatorImplementation{allocateImage,
                                                      destroyAllocator};
} // namespace
class ThumbnailReadbackTests final : public QObject {
  Q_OBJECT
private Q_SLOTS:
  void scalesActualPixels() {
    auto *renderer = wlr_pixman_renderer_create();
    QVERIFY(renderer);
    auto *allocator = new wlr_allocator;
    wlr_allocator_init(allocator, &allocatorImplementation,
                       WLR_BUFFER_CAP_DATA_PTR);
    const uint8_t pixels[] = {
        255, 0, 0, 255, 255, 0, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255,
        255, 0, 0, 255, 255, 0, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255,
        0, 0, 255, 255, 0, 0, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
        0, 0, 255, 255, 0, 0, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255};
    auto *texture =
        wlr_texture_from_pixels(renderer, DRM_FORMAT_ABGR8888, 16, 4, 4, pixels);
    QVERIFY(texture);
    QByteArray result(8 * 8 * 4, '\0');
    QVERIFY(ludash_thumbnail_read(renderer, allocator, texture, nullptr, 0, 8,
                                  8,
                                  reinterpret_cast<uint8_t *>(result.data())));
    const auto byte = [&result](int offset) {
      return static_cast<uint8_t>(result[offset]);
    };
    // Check solid quadrants away from renderer-specific edge padding.
    const int top = (1 * 8 + 1) * 4;
    QCOMPARE(byte(top), 255);
    QCOMPARE(byte(top + 1), 0);
    QCOMPARE(byte(top + 2), 0);
    QCOMPARE(byte(top + 3), 255);
    const int bottom = (6 * 8 + 1) * 4;
    QCOMPARE(byte(bottom), 0);
    QCOMPARE(byte(bottom + 2), 255);
    QCOMPARE(byte(bottom + 3), 255);
    QVERIFY(!ludash_thumbnail_read(renderer, allocator, texture, nullptr, 0,
                                   10000, 8,
                                   reinterpret_cast<uint8_t *>(result.data())));
    wlr_texture_destroy(texture);
    wlr_allocator_destroy(allocator);
    wlr_renderer_destroy(renderer);
  }
};
} // namespace LunaDash
QTEST_GUILESS_MAIN(LunaDash::ThumbnailReadbackTests)
#include "ThumbnailReadbackTests.moc"
