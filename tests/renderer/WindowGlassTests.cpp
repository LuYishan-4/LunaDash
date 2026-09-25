#include "compositor/renderer/blur/WindowGlass.hpp"
#include <QByteArray>
#include <QVector>
#include <QtTest>
#include <cstring>
#include <drm_fourcc.h>
#include <memory>
#define static
extern "C" {
#include <wlr/interfaces/wlr_buffer.h>
#include <wlr/render/allocator.h>
#include <wlr/render/drm_format_set.h>
#include <wlr/render/pixman.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_scene.h>
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

ImageBuffer *createImage(int width, int height, uint32_t color = 0xff000000) {
  auto *image = new ImageBuffer;
  image->pixels.resize(width * height * 4);
  for (int offset = 0; offset < image->pixels.size(); offset += 4)
    std::memcpy(image->pixels.data() + offset, &color, 4);
  wlr_buffer_init(&image->base, &imageImplementation, width, height);
  return image;
}

struct TestAllocator {
  wlr_allocator base{};
  int calls = 0;
  int liveBuffers = 0;
};

wlr_buffer *allocateImage(wlr_allocator *base, int width, int height,
                          const wlr_drm_format *) {
  auto *allocator = reinterpret_cast<TestAllocator *>(base);
  ++allocator->calls;
  if (width < 1 || height < 1 || width > 1024 || height > 1024)
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

struct Window {
  wlr_scene_tree *tree = nullptr;
  wlr_scene_buffer *content = nullptr;
};

wlr_scene_tree *underlay(wlr_scene_tree *tree) {
  if (!tree->node.parent ||
      tree->node.link.prev == &tree->node.parent->children)
    return nullptr;
  wlr_scene_node *previous = nullptr;
  previous = wl_container_of(tree->node.link.prev, previous, link);
  if (previous->type != WLR_SCENE_NODE_TREE)
    return nullptr;
  auto *candidate = wlr_scene_tree_from_node(previous);
  if (wl_list_empty(&candidate->children))
    return nullptr;
  wlr_scene_node *child = nullptr;
  wl_list_for_each(child, &candidate->children, link) {
    if (child->type != WLR_SCENE_NODE_BUFFER)
      return nullptr;
    auto *part = wlr_scene_buffer_from_node(child);
    double x = 0, y = 0;
    if (!part->point_accepts_input || part->point_accepts_input(part, &x, &y))
      return nullptr;
  }
  return candidate;
}

int childCount(wlr_scene_tree *tree) {
  int count = 0;
  wlr_scene_node *child = nullptr;
  wl_list_for_each(child, &tree->children, link)
    ++count;
  return count;
}

constexpr float red[] = {1, 0, 0, 1};
constexpr float blue[] = {0, 0, 1, 1};
} // namespace

class WindowGlassTests final : public QObject {
  Q_OBJECT
private:
  wlr_renderer *renderer_ = nullptr;
  TestAllocator *allocator_ = nullptr;
  wlr_scene *scene_ = nullptr;
  wlr_scene_rect *background_ = nullptr;
  std::unique_ptr<WindowGlass> glass_;

  Window addWindow(int contentSize = 64) {
    Window window;
    window.tree = wlr_scene_tree_create(&scene_->tree);
    if (!window.tree)
      return window;
    auto *image = createImage(contentSize, contentSize, 0xff00ff00);
    window.content = wlr_scene_buffer_create(window.tree, &image->base);
    wlr_buffer_drop(&image->base);
    return window;
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
    background_ = wlr_scene_rect_create(&scene_->tree, 512, 512, red);
    QVERIFY(background_);
    glass_ = std::make_unique<WindowGlass>(renderer_, &allocator_->base,
                                         &scene_->tree.node);
    glass_->configure(true, 8, 0.8f);
  }

  void cleanup() {
    glass_.reset();
    if (scene_)
      wlr_scene_node_destroy(&scene_->tree.node);
    if (allocator_) {
      QCOMPARE(allocator_->liveBuffers, 0);
      wlr_allocator_destroy(&allocator_->base);
    }
    if (renderer_)
      wlr_renderer_destroy(renderer_);
    scene_ = nullptr;
    background_ = nullptr;
    allocator_ = nullptr;
    renderer_ = nullptr;
  }

  void unchangedSceneDoesNotRecapture() {
    const auto window = addWindow();
    QVERIFY(window.tree && window.content);
    const QList<WindowGlass::Surface> surfaces{{window.tree, {64, 64}, true}};
    glass_->update(surfaces, false);
    QVERIFY2(glass_->ready(), qPrintable(glass_->error()));
    QVERIFY(!glass_->failed());
    QCOMPARE(glass_->frames(), quint64{1});
    const int allocations = allocator_->calls;
    for (int frame = 0; frame < 5; ++frame) {
      glass_->update(surfaces, false);
      QVERIFY(glass_->ready());
    }
    QCOMPARE(glass_->frames(), quint64{1});
    QCOMPARE(allocator_->calls, allocations);
    QVERIFY(!glass_->configure(true, 8, 0.8f));
    glass_->update(surfaces, false);
    QCOMPARE(allocator_->calls, allocations);
    wlr_scene_rect_set_color(background_, blue);
    glass_->update(surfaces, false);
    QCOMPARE(glass_->frames(), quint64{2});
    QVERIFY(allocator_->calls > allocations);
    glass_->update(surfaces, false);
    QCOMPARE(glass_->frames(), quint64{2});
  }

  void geometryChangesRecaptureAndMoveTheUnderlay() {
    const auto window = addWindow();
    QVERIFY(window.tree && window.content);
    glass_->update({{window.tree, {64, 64}, true}}, false);
    QVERIFY(glass_->ready());
    QCOMPARE(glass_->frames(), quint64{1});
    wlr_scene_node_set_position(&window.tree->node, 24, 16);
    glass_->update({{window.tree, {64, 64}, true}}, false);
    QCOMPARE(glass_->frames(), quint64{2});
    auto *backdrop = underlay(window.tree);
    QVERIFY(backdrop);
    QCOMPARE(backdrop->node.x, 24);
    QCOMPARE(backdrop->node.y, 16);
    glass_->update({{window.tree, {96, 80}, true}}, false);
    QCOMPARE(glass_->frames(), quint64{3});
    QVERIFY(childCount(backdrop) > 1);
    QVector<int> coveredWidth(80, 0);
    int bottom = 0;
    wlr_scene_node *child = nullptr;
    wl_list_for_each(child, &backdrop->children, link) {
      QCOMPARE(child->type, WLR_SCENE_NODE_BUFFER);
      auto *part = wlr_scene_buffer_from_node(child);
      QVERIFY(part->buffer);
      QVERIFY(child->x >= 0 && child->y >= 0);
      QVERIFY(part->dst_width > 0 && part->dst_height > 0);
      QVERIFY(child->x + part->dst_width <= 96);
      QVERIFY(child->y + part->dst_height <= 80);
      QCOMPARE(child->y, bottom);
      bottom = child->y + part->dst_height;
      QCOMPARE(part->src_box.x, double(child->x) * part->buffer->width / 96);
      QCOMPARE(part->src_box.y, double(child->y) * part->buffer->height / 80);
      QCOMPARE(part->src_box.width,
               double(part->dst_width) * part->buffer->width / 96);
      QCOMPARE(part->src_box.height,
               double(part->dst_height) * part->buffer->height / 80);
      for (int row = child->y; row < bottom; ++row)
        coveredWidth[row] += part->dst_width;
    }
    QCOMPARE(bottom, 80);
    QVERIFY(coveredWidth.front() > 0 && coveredWidth.front() < 96);
    QCOMPARE(coveredWidth.front(), coveredWidth.back());
    QCOMPARE(coveredWidth[40], 96);
    glass_->update({{window.tree, {96, 80}, true}}, false);
    QCOMPARE(glass_->frames(), quint64{3});
  }

  void offAreaActivityDoesNotInvalidateBackdrops() {
    auto *activity = wlr_scene_tree_create(&scene_->tree);
    QVERIFY(activity);
    auto *indicator = wlr_scene_rect_create(activity, 16, 16, blue);
    QVERIFY(indicator);
    wlr_scene_node_set_position(&activity->node, 300, 200);
    const auto window = addWindow();
    const QList<WindowGlass::Surface> surfaces{{window.tree, {64, 64}, true}};
    glass_->update(surfaces, false);
    QCOMPARE(glass_->frames(), quint64{1});
    const int allocations = allocator_->calls;
    const int live = allocator_->liveBuffers;
    for (int frame = 0; frame < 600; ++frame) {
      wlr_scene_rect_set_color(indicator, frame % 2 ? red : blue);
      wlr_scene_node_set_position(&activity->node, 300 + frame % 20, 200);
      glass_->update(surfaces, false);
    }
    QCOMPARE(glass_->frames(), quint64{1});
    QCOMPARE(allocator_->calls, allocations);
    QCOMPARE(allocator_->liveBuffers, live);
    // The filter samples beyond the window edge. Moving activity into that
    // padding must invalidate, even without overlapping the visible tile.
    wlr_scene_node_set_position(&activity->node, 66, 10);
    glass_->update(surfaces, false);
    QCOMPARE(glass_->frames(), quint64{2});
    wlr_scene_node_set_position(&activity->node, 300, 200);
    glass_->update(surfaces, false);
    QCOMPARE(glass_->frames(), quint64{3});
    // Removing an unrelated subtree must not invalidate the cache either.
    wlr_scene_node_destroy(&activity->node);
    glass_->update(surfaces, false);
    QCOMPARE(glass_->frames(), quint64{3});
  }

  void animationRetainsAlphaAndDisablingRestoresSteadyState() {
    const auto window = addWindow();
    QVERIFY(window.tree && window.content);
    const QList<WindowGlass::Surface> surfaces{{window.tree, {64, 64}, true}};
    glass_->update(surfaces, false);
    QCOMPARE(window.content->opacity, 0.8f);
    wlr_scene_buffer_set_opacity(window.content, 0.25f);
    glass_->update(surfaces, true);
    QCOMPARE(window.content->opacity, 0.25f);
    QVERIFY(glass_->configure(false, 8, 1.0f));
    glass_->update(surfaces, true);
    QCOMPARE(window.content->opacity, 0.25f);
    QVERIFY(!glass_->ready());
    QVERIFY(!underlay(window.tree));
    QCOMPARE(allocator_->liveBuffers, 0);
    glass_->update(surfaces, false);
    QCOMPARE(window.content->opacity, 1.0f);
    QVERIFY(!glass_->failed());
    QVERIFY(glass_->error().isEmpty());
    // Reduced/eye-care configurations can also suppress blur with a zero radius.
    glass_->configure(true, 0, 1.0f);
    glass_->update(surfaces, false);
    QVERIFY(!glass_->ready());
    QVERIFY(!underlay(window.tree));
    QCOMPARE(window.content->opacity, 1.0f);
  }

  void invisibleAndExcludedWindowsHaveNoBackdrop() {
    const auto window = addWindow();
    QVERIFY(window.tree && window.content);
    wlr_scene_node_set_enabled(&window.tree->node, false);
    glass_->update({{window.tree, {64, 64}, true}}, false);
    QVERIFY(!glass_->ready());
    QCOMPARE(allocator_->calls, 0);
    QVERIFY(!underlay(window.tree));
    wlr_scene_node_set_enabled(&window.tree->node, true);
    glass_->update({{window.tree, {64, 64}, false}}, false);
    QVERIFY(!glass_->ready());
    QCOMPARE(window.content->opacity, 1.0f);
    QCOMPARE(allocator_->calls, 0);
    glass_->update({{window.tree, {64, 64}, true}}, false);
    QVERIFY(glass_->ready());
    wlr_scene_node_set_enabled(&window.tree->node, false);
    glass_->update({{window.tree, {64, 64}, true}}, false);
    QVERIFY(!glass_->ready());
    QVERIFY(!underlay(window.tree));
    QCOMPARE(allocator_->liveBuffers, 0);
  }

  void targetAndRootDestructionReleaseUnderlays() {
    const auto window = addWindow();
    QVERIFY(window.tree && window.content);
    glass_->update({{window.tree, {64, 64}, true}}, false);
    QVERIFY(glass_->ready());
    QCOMPARE(childCount(&scene_->tree), 3);
    wlr_scene_node_destroy(&window.tree->node);
    QCOMPARE(childCount(&scene_->tree), 1);
    QCOMPARE(allocator_->liveBuffers, 0);
    glass_->update({}, false);
    QVERIFY(!glass_->ready());
    const auto replacement = addWindow();
    QVERIFY(replacement.tree && replacement.content);
    glass_->update({{replacement.tree, {64, 64}, true}}, false);
    QVERIFY(glass_->ready());
    wlr_scene_node_destroy(&scene_->tree.node);
    scene_ = nullptr;
    background_ = nullptr;
    QCOMPARE(allocator_->liveBuffers, 0);
    glass_->update({}, false);
    QVERIFY(!glass_->ready());
    QVERIFY(!glass_->failed());
    glass_.reset();
  }

  void restackingAndReparentingKeepEachUnderlayBelowItsTarget() {
    const auto first = addWindow();
    const auto second = addWindow();
    QVERIFY(first.tree && first.content && second.tree && second.content);
    // Input order is deliberately the reverse of scene painter order.
    const QList<WindowGlass::Surface> surfaces{
        {second.tree, {64, 64}, true}, {first.tree, {64, 64}, true}};
    glass_->update(surfaces, false);
    QVERIFY2(glass_->ready(), qPrintable(glass_->error()));
    auto *firstBackdrop = underlay(first.tree);
    auto *secondBackdrop = underlay(second.tree);
    QVERIFY(firstBackdrop && secondBackdrop && firstBackdrop != secondBackdrop);
    QCOMPARE(glass_->frames(), quint64{2});
    glass_->update(surfaces, false);
    QCOMPARE(glass_->frames(), quint64{2});
    wlr_scene_node_raise_to_top(&first.tree->node);
    glass_->update(surfaces, false);
    QCOMPARE(underlay(first.tree), firstBackdrop);
    QCOMPARE(underlay(second.tree), secondBackdrop);
    QVERIFY(glass_->frames() > 2);
    const auto restacked = glass_->frames();
    glass_->update(surfaces, false);
    QCOMPARE(glass_->frames(), restacked);
    auto *workspace = wlr_scene_tree_create(&scene_->tree);
    QVERIFY(workspace);
    wlr_scene_node_set_position(&workspace->node, 80, 40);
    wlr_scene_node_reparent(&first.tree->node, workspace);
    glass_->update(surfaces, false);
    QCOMPARE(underlay(first.tree), firstBackdrop);
    QCOMPARE(firstBackdrop->node.parent, workspace);
    QCOMPARE(secondBackdrop->node.parent, &scene_->tree);
    QVERIFY(glass_->frames() > restacked);
  }

  void underlayNeverInterceptsClientInput() {
    const auto window = addWindow(32);
    QVERIFY(window.tree && window.content);
    glass_->update({{window.tree, {64, 64}, true}}, false);
    auto *backdrop = underlay(window.tree);
    QVERIFY(backdrop);
    wlr_scene_node *child = nullptr;
    wl_list_for_each(child, &backdrop->children, link) {
      auto *part = wlr_scene_buffer_from_node(child);
      QVERIFY(part->point_accepts_input);
      double x = 0, y = 0;
      QVERIFY(!part->point_accepts_input(part, &x, &y));
    }
    double localX = 0, localY = 0;
    QCOMPARE(wlr_scene_node_at(&scene_->tree.node, 16, 16, &localX, &localY),
             &window.content->node);
    // The larger glass rectangle must also let input through outside content.
    QCOMPARE(wlr_scene_node_at(&scene_->tree.node, 48, 48, &localX, &localY),
             &background_->node);
  }
};
} // namespace LunaDash

QTEST_GUILESS_MAIN(LunaDash::WindowGlassTests)
#include "WindowGlassTests.moc"
