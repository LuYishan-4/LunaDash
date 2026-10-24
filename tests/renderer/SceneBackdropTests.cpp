#include "compositor/renderer/blur/SceneBackdrop.h"
#include "support/TestHarness.hpp"
#include <string>
#include <vector>
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
  std::string pixels;
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
  image->pixels = std::string(width * height * 4, '\0');
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
  std::string pixels;

  int channel(int x, int y, int shift) const {
    uint32_t pixel = 0;
    std::memcpy(&pixel, pixels.data() + (y * width + x) * 4, 4);
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

class SceneBackdropTests final {
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

public:
  void init() {
    renderer_ = wlr_pixman_renderer_create();
    VERIFY(renderer_);
    allocator_ = new TestAllocator;
    wlr_allocator_init(&allocator_->base, &allocatorImplementation,
                       WLR_BUFFER_CAP_DATA_PTR);
    scene_ = wlr_scene_create();
    VERIFY(scene_);
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
    VERIFY(surface);
    auto *stop = wlr_scene_tree_create(&scene_->tree);
    VERIFY(stop);
    const auto image = capture(&stop->node);
    COMPARE(image.width, 16);
    COMPARE(image.height, 16);
    VERIFY(image.channel(2, 2, 16) < 30);
    VERIFY(image.channel(13, 13, 16) > 220);
    // These samples are inside the original black pixels next to each edge.
    // Downscaling alone leaves them black; a two-axis blur mixes the white side.
    VERIFY(image.channel(7, 12, 16) > 5);
    VERIFY(image.channel(7, 12, 16) < 245);
    VERIFY(image.channel(12, 7, 16) > 5);
    VERIFY(image.channel(12, 7, 16) < 245);
    COMPARE(image.channel(7, 12, 24), 255);
  }

  void excludesTargetSubtreeAndHigherWindows() {
    VERIFY(wlr_scene_rect_create(&scene_->tree, 64, 64, red));
    auto *target = wlr_scene_tree_create(&scene_->tree);
    VERIFY(target);
    VERIFY(wlr_scene_rect_create(target, 64, 64, blue));
    VERIFY(wlr_scene_rect_create(&scene_->tree, 64, 64, green));
    const auto image = capture(&target->node);
    VERIFY(!image.pixels.empty());
    COMPARE(image.channel(8, 8, 16), 255);
    COMPARE(image.channel(8, 8, 8), 0);
    COMPARE(image.channel(8, 8, 0), 0);
  }

  void doesNotCaptureItsPreviousBackdrop() {
    auto *background = wlr_scene_rect_create(&scene_->tree, 64, 64, red);
    VERIFY(background);
    auto *target = wlr_scene_tree_create(&scene_->tree);
    VERIFY(target);
    const wlr_box area{0, 0, 64, 64};
    auto *first = ludash_scene_backdrop_render(
        renderer_, &allocator_->base, &scene_->tree.node, &target->node,
        nullptr, &area, 8);
    VERIFY(first);
    auto *previous = wlr_scene_buffer_create(&scene_->tree, first);
    wlr_buffer_drop(first);
    VERIFY(previous);
    wlr_scene_buffer_set_dest_size(previous, 64, 64);
    wlr_scene_node_place_below(&previous->node, &target->node);
    wlr_scene_rect_set_color(background, blue);
    for (int attempt = 0; attempt < 3; ++attempt) {
      const auto image = capture(&target->node, &previous->node);
      VERIFY(!image.pixels.empty());
      COMPARE(image.channel(8, 8, 16), 0);
      COMPARE(image.channel(8, 8, 0), 255);
    }
  }

  void skipsDisabledNodesAndDisabledAncestors() {
    VERIFY(wlr_scene_rect_create(&scene_->tree, 64, 64, red));
    auto *hiddenTree = wlr_scene_tree_create(&scene_->tree);
    VERIFY(hiddenTree);
    VERIFY(wlr_scene_rect_create(hiddenTree, 64, 64, blue));
    wlr_scene_node_set_enabled(&hiddenTree->node, false);
    auto *hiddenRect = wlr_scene_rect_create(&scene_->tree, 64, 64, green);
    VERIFY(hiddenRect);
    wlr_scene_node_set_enabled(&hiddenRect->node, false);
    auto *stop = wlr_scene_tree_create(&scene_->tree);
    VERIFY(stop);
    const auto image = capture(&stop->node);
    VERIFY(!image.pixels.empty());
    COMPARE(image.channel(8, 8, 16), 255);
    COMPARE(image.channel(8, 8, 8), 0);
    COMPARE(image.channel(8, 8, 0), 0);
  }

  void cropsLogicalAreaWithNestedCoordinates() {
    VERIFY(wlr_scene_rect_create(&scene_->tree, 64, 64, red));
    auto *nested = wlr_scene_tree_create(&scene_->tree);
    VERIFY(nested);
    wlr_scene_node_set_position(&nested->node, 16, 8);
    VERIFY(wlr_scene_rect_create(nested, 16, 32, white));
    auto *stop = wlr_scene_tree_create(&scene_->tree);
    VERIFY(stop);
    const auto image = capture(&stop->node, nullptr, {16, 8, 32, 32}, 1);
    COMPARE(image.width, 8);
    COMPARE(image.height, 8);
    COMPARE(image.channel(1, 4, 16), 255);
    VERIFY(image.channel(1, 4, 8) > 240);
    COMPARE(image.channel(6, 4, 16), 255);
    VERIFY(image.channel(6, 4, 8) < 15);
  }

  void rejectsInvalidArgumentsBeforeAllocation() {
    auto *stop = wlr_scene_tree_create(&scene_->tree);
    VERIFY(stop);
    const wlr_box area{0, 0, 64, 64};
    VERIFY(!ludash_scene_backdrop_render(nullptr, &allocator_->base,
                                         &scene_->tree.node, &stop->node,
                                         nullptr, &area, 8));
    VERIFY(!ludash_scene_backdrop_render(renderer_, nullptr,
                                         &scene_->tree.node, &stop->node,
                                         nullptr, &area, 8));
    VERIFY(!ludash_scene_backdrop_render(renderer_, &allocator_->base,
                                         nullptr, &stop->node, nullptr, &area,
                                         8));
    VERIFY(!ludash_scene_backdrop_render(renderer_, &allocator_->base,
                                         &scene_->tree.node, &stop->node,
                                         nullptr, nullptr, 8));
    VERIFY(!ludash_scene_backdrop_render(renderer_, &allocator_->base,
                                         &scene_->tree.node, nullptr, nullptr,
                                         &area, 8));
    VERIFY(!ludash_scene_backdrop_render(
        renderer_, &allocator_->base, &scene_->tree.node, &scene_->tree.node,
        nullptr, &area, 8));
    VERIFY(!ludash_scene_backdrop_render(
        renderer_, &allocator_->base, &scene_->tree.node, &stop->node,
        &stop->node, &area, 8));
    for (const auto invalid : {wlr_box{0, 0, 0, 64}, wlr_box{0, 0, 64, -1},
                               wlr_box{0, 0, 16385, 64},
                               wlr_box{0, 0, 64, 16385},
                               wlr_box{-65537, 0, 64, 64},
                               wlr_box{0, 65537, 64, 64}}) {
      VERIFY(!ludash_scene_backdrop_render(
          renderer_, &allocator_->base, &scene_->tree.node, &stop->node,
          nullptr, &invalid, 8));
    }
    for (const int radius : {0, -1, 33}) {
      VERIFY(!ludash_scene_backdrop_render(
          renderer_, &allocator_->base, &scene_->tree.node, &stop->node,
          nullptr, &area, radius));
    }
    COMPARE(allocator_->calls, 0);
  }

  void missingStopDoesNotLeakPartialCapture() {
    VERIFY(wlr_scene_rect_create(&scene_->tree, 64, 64, red));
    auto *foreign = wlr_scene_create();
    VERIFY(foreign);
    const auto image = capture(&foreign->tree.node);
    wlr_scene_node_destroy(&foreign->tree.node);
    VERIFY(image.pixels.empty());
    COMPARE(allocator_->liveBuffers, 0);
  }

  void capsAllocationsAndPropagatesAllocationFailure() {
    VERIFY(wlr_scene_rect_create(&scene_->tree, 16384, 16, black));
    auto *stop = wlr_scene_tree_create(&scene_->tree);
    VERIFY(stop);
    for (const auto area : {wlr_box{0, 0, 16384, 16},
                            wlr_box{0, 0, 16384, 1},
                            wlr_box{0, 0, 1, 16384}}) {
      const auto image = capture(&stop->node, nullptr, area, 32);
      VERIFY(!image.pixels.empty());
      VERIFY(image.width <= 1024);
      VERIFY(image.height <= 1024);
      COMPARE(allocator_->liveBuffers, 0);
    }
    VERIFY(allocator_->largestWidth <= 1024);
    VERIFY(allocator_->largestHeight <= 1024);
    allocator_->failAt = allocator_->calls + 2;
    VERIFY(capture(&stop->node).pixels.empty());
    COMPARE(allocator_->liveBuffers, 0);
    allocator_->fail = true;
    VERIFY(capture(&stop->node).pixels.empty());
    COMPARE(allocator_->liveBuffers, 0);
  }
};
} // namespace LunaDash

int main(int argc, char **argv) {
  return LunaDash::Test::run<LunaDash::SceneBackdropTests>(
      "SceneBackdropTests",
      {
      {"init", &LunaDash::SceneBackdropTests::init},
      {"cleanup", &LunaDash::SceneBackdropTests::cleanup},
      {"blursTextureEdgesInBothAxes", &LunaDash::SceneBackdropTests::blursTextureEdgesInBothAxes},
      {"excludesTargetSubtreeAndHigherWindows", &LunaDash::SceneBackdropTests::excludesTargetSubtreeAndHigherWindows},
      {"doesNotCaptureItsPreviousBackdrop", &LunaDash::SceneBackdropTests::doesNotCaptureItsPreviousBackdrop},
      {"skipsDisabledNodesAndDisabledAncestors", &LunaDash::SceneBackdropTests::skipsDisabledNodesAndDisabledAncestors},
      {"cropsLogicalAreaWithNestedCoordinates", &LunaDash::SceneBackdropTests::cropsLogicalAreaWithNestedCoordinates},
      {"rejectsInvalidArgumentsBeforeAllocation", &LunaDash::SceneBackdropTests::rejectsInvalidArgumentsBeforeAllocation},
      {"missingStopDoesNotLeakPartialCapture", &LunaDash::SceneBackdropTests::missingStopDoesNotLeakPartialCapture},
      {"capsAllocationsAndPropagatesAllocationFailure", &LunaDash::SceneBackdropTests::capsAllocationsAndPropagatesAllocationFailure}},
      argc, argv);
}
