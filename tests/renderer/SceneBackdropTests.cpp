#include "compositor/renderer/blur/SceneBackdrop.h"
#include <QByteArray>
#include <QtTest>
#include <algorithm>
#include <cstring>
#include <drm_fourcc.h>
#define static
extern "C" {
#include <wlr/interfaces/wlr_buffer.h>
#include <wlr/render/allocator.h>
#include <wlr/render/drm_format_set.h>
#include <wlr/render/pixman.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/util/box.h>
#include <wlr/version.h>
}
#undef static

namespace LunaDash {
namespace {
struct ImageBuffer {
  wlr_buffer base{};
  QByteArray pixels;
  int *liveBuffers = nullptr;
};

void destroyImage(wlr_buffer *buffer) {
#if WLR_VERSION_MINOR >= 19
  wlr_buffer_finish(buffer);
#endif
  auto *image = reinterpret_cast<ImageBuffer *>(buffer);
  if (image->liveBuffers)
    --*image->liveBuffers;
  delete image;
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

const wlr_buffer_impl imageImplementation{
    .destroy = destroyImage,
    .get_dmabuf = nullptr,
    .get_shm = nullptr,
    .begin_data_ptr_access = accessImage,
    .end_data_ptr_access = finishAccessImage};

ImageBuffer *createImage(int width, int height) {
  auto *image = new ImageBuffer;
  image->pixels = QByteArray(width * height * 4, '\0');
  wlr_buffer_init(&image->base, &imageImplementation, width, height);
  return image;
}

struct TestAllocator {
  wlr_allocator base{};
  int calls = 0;
  int largestWidth = 0;
  int largestHeight = 0;
  int liveBuffers = 0;
  int failAt = 0;
  bool fail = false;
};

wlr_buffer *allocateImage(wlr_allocator *base, int width, int height,
                          const wlr_drm_format *) {
  auto *allocator = reinterpret_cast<TestAllocator *>(base);
  ++allocator->calls;
  allocator->largestWidth = std::max(allocator->largestWidth, width);
  allocator->largestHeight = std::max(allocator->largestHeight, height);
  if (allocator->fail || allocator->calls == allocator->failAt ||
      width < 1 || height < 1 || width > 1024 ||
      height > 1024)
    return nullptr;
  auto *image = createImage(width, height);
  image->liveBuffers = &allocator->liveBuffers;
  ++allocator->liveBuffers;
  return &image->base;
}

void destroyAllocator(wlr_allocator *allocator) {
  delete reinterpret_cast<TestAllocator *>(allocator);
}

const wlr_allocator_interface allocatorImplementation{allocateImage,
                                                       destroyAllocator};

struct Readback {
  int width = 0;
  int height = 0;
  QByteArray pixels;

  int channel(int x, int y, int shift) const {
    uint32_t pixel = 0;
    std::memcpy(&pixel, pixels.constData() + (y * width + x) * 4, 4);
    return static_cast<int>((pixel >> shift) & 255);
  }
};

Readback readImage(wlr_buffer *buffer) {
  Readback image;
  if (!buffer)
    return image;
  void *data = nullptr;
  uint32_t format = 0;
  size_t stride = 0;
  if (wlr_buffer_begin_data_ptr_access(buffer, WLR_BUFFER_DATA_PTR_ACCESS_READ,
                                      &data, &format, &stride)) {
    if (format == DRM_FORMAT_ARGB8888) {
      image.width = buffer->width;
      image.height = buffer->height;
      image.pixels.resize(image.width * image.height * 4);
      for (int row = 0; row < image.height; ++row)
        std::memcpy(image.pixels.data() + row * image.width * 4,
                    static_cast<const char *>(data) + row * stride,
                    image.width * 4);
    }
    wlr_buffer_end_data_ptr_access(buffer);
  }
  wlr_buffer_drop(buffer);
  return image;
}

constexpr float red[] = {1, 0, 0, 1};
constexpr float blue[] = {0, 0, 1, 1};
constexpr float green[] = {0, 1, 0, 1};
constexpr float black[] = {0, 0, 0, 1};
constexpr float white[] = {1, 1, 1, 1};
} // namespace

class SceneBackdropTests final : public QObject {
  Q_OBJECT
private:
  wlr_renderer *renderer_ = nullptr;
  TestAllocator *allocator_ = nullptr;
  wlr_scene *scene_ = nullptr;

  Readback capture(wlr_scene_node *stop, wlr_scene_node *skip = nullptr,
                   const wlr_box &area = {0, 0, 64, 64}, int radius = 8) {
    return readImage(ludash_scene_backdrop_render(
        renderer_, &allocator_->base, &scene_->tree.node, stop, skip, &area,
        radius));
  }

private Q_SLOTS:
  void init() {
    renderer_ = wlr_pixman_renderer_create();
    QVERIFY(renderer_);
    allocator_ = new TestAllocator;
    wlr_allocator_init(&allocator_->base, &allocatorImplementation,
                       WLR_BUFFER_CAP_DATA_PTR);
    scene_ = wlr_scene_create();
    QVERIFY(scene_);
  }

  void cleanup() {
    if (scene_)
      wlr_scene_node_destroy(&scene_->tree.node);
    if (allocator_)
      wlr_allocator_destroy(&allocator_->base);
    if (renderer_)
      wlr_renderer_destroy(renderer_);
    scene_ = nullptr;
    allocator_ = nullptr;
    renderer_ = nullptr;
  }

  void blursTextureEdgesInBothAxes() {
    auto *source = createImage(64, 64);
    for (int y = 0; y < 64; ++y) {
      for (int x = 0; x < 64; ++x) {
        const uint32_t color = x >= 32 && y >= 32 ? 0xffffffff : 0xff000000;
        std::memcpy(source->pixels.data() + (y * 64 + x) * 4, &color, 4);
      }
    }
    auto *surface = wlr_scene_buffer_create(&scene_->tree, &source->base);
    wlr_buffer_drop(&source->base);
    QVERIFY(surface);
    auto *stop = wlr_scene_tree_create(&scene_->tree);
    QVERIFY(stop);
    const auto image = capture(&stop->node);
    QCOMPARE(image.width, 16);
    QCOMPARE(image.height, 16);
    QVERIFY(image.channel(2, 2, 16) < 30);
    QVERIFY(image.channel(13, 13, 16) > 220);
    // These samples are inside the original black pixels next to each edge.
    // Downscaling alone leaves them black; a two-axis blur mixes the white side.
    QVERIFY(image.channel(7, 12, 16) > 5);
    QVERIFY(image.channel(7, 12, 16) < 245);
    QVERIFY(image.channel(12, 7, 16) > 5);
    QVERIFY(image.channel(12, 7, 16) < 245);
    QCOMPARE(image.channel(7, 12, 24), 255);
  }

  void excludesTargetSubtreeAndHigherWindows() {
    QVERIFY(wlr_scene_rect_create(&scene_->tree, 64, 64, red));
    auto *target = wlr_scene_tree_create(&scene_->tree);
    QVERIFY(target);
    QVERIFY(wlr_scene_rect_create(target, 64, 64, blue));
    QVERIFY(wlr_scene_rect_create(&scene_->tree, 64, 64, green));
    const auto image = capture(&target->node);
    QVERIFY(!image.pixels.isEmpty());
    QCOMPARE(image.channel(8, 8, 16), 255);
    QCOMPARE(image.channel(8, 8, 8), 0);
    QCOMPARE(image.channel(8, 8, 0), 0);
  }

  void doesNotCaptureItsPreviousBackdrop() {
    auto *background = wlr_scene_rect_create(&scene_->tree, 64, 64, red);
    QVERIFY(background);
    auto *target = wlr_scene_tree_create(&scene_->tree);
    QVERIFY(target);
    const wlr_box area{0, 0, 64, 64};
    auto *first = ludash_scene_backdrop_render(
        renderer_, &allocator_->base, &scene_->tree.node, &target->node,
        nullptr, &area, 8);
    QVERIFY(first);
    auto *previous = wlr_scene_buffer_create(&scene_->tree, first);
    wlr_buffer_drop(first);
    QVERIFY(previous);
    wlr_scene_buffer_set_dest_size(previous, 64, 64);
    wlr_scene_node_place_below(&previous->node, &target->node);
    wlr_scene_rect_set_color(background, blue);
    for (int attempt = 0; attempt < 3; ++attempt) {
      const auto image = capture(&target->node, &previous->node);
      QVERIFY(!image.pixels.isEmpty());
      QCOMPARE(image.channel(8, 8, 16), 0);
      QCOMPARE(image.channel(8, 8, 0), 255);
    }
  }

  void skipsDisabledNodesAndDisabledAncestors() {
    QVERIFY(wlr_scene_rect_create(&scene_->tree, 64, 64, red));
    auto *hiddenTree = wlr_scene_tree_create(&scene_->tree);
    QVERIFY(hiddenTree);
    QVERIFY(wlr_scene_rect_create(hiddenTree, 64, 64, blue));
    wlr_scene_node_set_enabled(&hiddenTree->node, false);
    auto *hiddenRect = wlr_scene_rect_create(&scene_->tree, 64, 64, green);
    QVERIFY(hiddenRect);
    wlr_scene_node_set_enabled(&hiddenRect->node, false);
    auto *stop = wlr_scene_tree_create(&scene_->tree);
    QVERIFY(stop);
    const auto image = capture(&stop->node);
    QVERIFY(!image.pixels.isEmpty());
    QCOMPARE(image.channel(8, 8, 16), 255);
    QCOMPARE(image.channel(8, 8, 8), 0);
    QCOMPARE(image.channel(8, 8, 0), 0);
  }

  void cropsLogicalAreaWithNestedCoordinates() {
    QVERIFY(wlr_scene_rect_create(&scene_->tree, 64, 64, red));
    auto *nested = wlr_scene_tree_create(&scene_->tree);
    QVERIFY(nested);
    wlr_scene_node_set_position(&nested->node, 16, 8);
    QVERIFY(wlr_scene_rect_create(nested, 16, 32, white));
    auto *stop = wlr_scene_tree_create(&scene_->tree);
    QVERIFY(stop);
    const auto image = capture(&stop->node, nullptr, {16, 8, 32, 32}, 1);
    QCOMPARE(image.width, 8);
    QCOMPARE(image.height, 8);
    QCOMPARE(image.channel(1, 4, 16), 255);
    QVERIFY(image.channel(1, 4, 8) > 240);
    QCOMPARE(image.channel(6, 4, 16), 255);
    QVERIFY(image.channel(6, 4, 8) < 15);
  }

  void rejectsInvalidArgumentsBeforeAllocation() {
    auto *stop = wlr_scene_tree_create(&scene_->tree);
    QVERIFY(stop);
    const wlr_box area{0, 0, 64, 64};
    QVERIFY(!ludash_scene_backdrop_render(nullptr, &allocator_->base,
                                         &scene_->tree.node, &stop->node,
                                         nullptr, &area, 8));
    QVERIFY(!ludash_scene_backdrop_render(renderer_, nullptr,
                                         &scene_->tree.node, &stop->node,
                                         nullptr, &area, 8));
    QVERIFY(!ludash_scene_backdrop_render(renderer_, &allocator_->base,
                                         nullptr, &stop->node, nullptr, &area,
                                         8));
    QVERIFY(!ludash_scene_backdrop_render(renderer_, &allocator_->base,
                                         &scene_->tree.node, &stop->node,
                                         nullptr, nullptr, 8));
    QVERIFY(!ludash_scene_backdrop_render(renderer_, &allocator_->base,
                                         &scene_->tree.node, nullptr, nullptr,
                                         &area, 8));
    QVERIFY(!ludash_scene_backdrop_render(
        renderer_, &allocator_->base, &scene_->tree.node, &scene_->tree.node,
        nullptr, &area, 8));
    QVERIFY(!ludash_scene_backdrop_render(
        renderer_, &allocator_->base, &scene_->tree.node, &stop->node,
        &stop->node, &area, 8));
    for (const auto invalid : {wlr_box{0, 0, 0, 64}, wlr_box{0, 0, 64, -1},
                               wlr_box{0, 0, 16385, 64},
                               wlr_box{0, 0, 64, 16385},
                               wlr_box{-65537, 0, 64, 64},
                               wlr_box{0, 65537, 64, 64}}) {
      QVERIFY(!ludash_scene_backdrop_render(
          renderer_, &allocator_->base, &scene_->tree.node, &stop->node,
          nullptr, &invalid, 8));
    }
    for (const int radius : {0, -1, 33}) {
      QVERIFY(!ludash_scene_backdrop_render(
          renderer_, &allocator_->base, &scene_->tree.node, &stop->node,
          nullptr, &area, radius));
    }
    QCOMPARE(allocator_->calls, 0);
  }

  void missingStopDoesNotLeakPartialCapture() {
    QVERIFY(wlr_scene_rect_create(&scene_->tree, 64, 64, red));
    auto *foreign = wlr_scene_create();
    QVERIFY(foreign);
    const auto image = capture(&foreign->tree.node);
    wlr_scene_node_destroy(&foreign->tree.node);
    QVERIFY(image.pixels.isEmpty());
    QCOMPARE(allocator_->liveBuffers, 0);
  }

  void capsAllocationsAndPropagatesAllocationFailure() {
    QVERIFY(wlr_scene_rect_create(&scene_->tree, 16384, 16, black));
    auto *stop = wlr_scene_tree_create(&scene_->tree);
    QVERIFY(stop);
    for (const auto area : {wlr_box{0, 0, 16384, 16},
                            wlr_box{0, 0, 16384, 1},
                            wlr_box{0, 0, 1, 16384}}) {
      const auto image = capture(&stop->node, nullptr, area, 32);
      QVERIFY(!image.pixels.isEmpty());
      QVERIFY(image.width <= 1024);
      QVERIFY(image.height <= 1024);
      QCOMPARE(allocator_->liveBuffers, 0);
    }
    QVERIFY(allocator_->largestWidth <= 1024);
    QVERIFY(allocator_->largestHeight <= 1024);
    allocator_->failAt = allocator_->calls + 2;
    QVERIFY(capture(&stop->node).pixels.isEmpty());
    QCOMPARE(allocator_->liveBuffers, 0);
    allocator_->fail = true;
    QVERIFY(capture(&stop->node).pixels.isEmpty());
    QCOMPARE(allocator_->liveBuffers, 0);
  }
};
} // namespace LunaDash

QTEST_GUILESS_MAIN(LunaDash::SceneBackdropTests)
#include "SceneBackdropTests.moc"
