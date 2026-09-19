#include "compositor/animation/SceneWindowAnimations.hpp"
#include "compositor/wayland/wlroots/WlrootsSceneHeaders.hpp"

#include <QCoreApplication>
#include <QThread>

extern "C" {
#include <wlr/version.h>
#if __has_include(<wlr/interfaces/wlr_buffer.h>)
#include <wlr/interfaces/wlr_buffer.h>
#endif
}

namespace LunaDash {
namespace {
void check(bool value, const char *message) {
  if (!value)
    qFatal("%s", message);
}

struct Buffer {
  wlr_buffer base;
};
int liveBuffers = 0;

void destroyBuffer(wlr_buffer *buffer) {
  --liveBuffers;
#if WLR_VERSION_MINOR >= 19
  wlr_buffer_finish(buffer);
#endif
  delete reinterpret_cast<Buffer *>(buffer);
}

const wlr_buffer_impl bufferImplementation = {.destroy = destroyBuffer};

wlr_scene_buffer *addBuffer(wlr_scene_tree *tree, int width, int height) {
  auto *storage = new Buffer;
  ++liveBuffers;
  wlr_buffer_init(&storage->base, &bufferImplementation, width, height);
  auto *buffer = wlr_scene_buffer_create(tree, &storage->base);
  wlr_buffer_drop(&storage->base);
  return buffer;
}

void advance(SceneWindowAnimations &animations, unsigned long delay) {
  QThread::msleep(delay);
  animations.advance();
}
} // namespace

void testSceneAnimations() {
  SceneWindowAnimations animations;
  animations.setDuration(600);
  auto *scene = wlr_scene_create();
  auto *normal = wlr_scene_tree_create(&scene->tree);
  auto *overlay = wlr_scene_tree_create(&scene->tree);
  auto *window = wlr_scene_tree_create(normal);
  auto *buffer = addBuffer(window, 120, 80);
  const QRect initial(20, 30, 120, 80);
  wlr_scene_node_set_position(&window->node, 20, 30);
  const QRect moved(400, 30, 120, 80);
  animations.setGeometry(window, initial, moved, overlay);
  check(window->node.x == 20 && animations.activeCount() == 1,
        "Selection starts from the rendered position instead of jumping");
  advance(animations, 120);
  check(window->node.x > 20 && window->node.x < 400,
        "Selection presents intermediate positions");
  const int interruptedX = window->node.x;
  const QRect retarget(60, 30, 120, 80);
  animations.setGeometry(window, moved, retarget, overlay);
  check(window->node.x == interruptedX,
        "Rapid switching starts at the current frame");
  advance(animations, 650);
  check(window->node.x == 60 && animations.activeCount() == 0,
        "Movement finishes exactly at the latest target");

  const QRect expanded(0, 0, 240, 160);
  animations.setGeometry(window, retarget, expanded, overlay);
  advance(animations, 120);
  const auto visible = animations.visualGeometry(window, expanded);
  check(visible.width() > 120 && visible.width() < 240,
        "Expansion presents intermediate sizes");
  check(buffer->dst_width == 0 && buffer->opacity < 1.0,
        "Resize preview does not repeatedly resize the client surface");
  double x = 0, y = 0;
  check(wlr_scene_node_at(&scene->tree.node, 50, 40, &x, &y) == &buffer->node,
        "Resize preview does not intercept pointer input");
  animations.setDuration(0);
  check(animations.activeCount() == 0 && buffer->opacity == 1.0 &&
            wl_list_empty(&overlay->children),
        "Reduced motion immediately removes previews and restores opacity");

  animations.setDuration(600);
  animations.hideSnapshot(window, overlay);
  wlr_scene_node_destroy(&window->node);
  check(animations.activeCount() == 1,
        "Close snapshot outlives the destroyed application surface");
  advance(animations, 320);
  wlr_scene_node *snapshotNode = nullptr;
  snapshotNode = wl_container_of(overlay->children.next, snapshotNode, link);
  auto *snapshot = wlr_scene_tree_from_node(snapshotNode);
  wlr_scene_node *copyNode = nullptr;
  copyNode = wl_container_of(snapshot->children.next, copyNode, link);
  auto *copy = wlr_scene_buffer_from_node(copyNode);
  check(copy->dst_width < 120 && copy->opacity > 0.0 && copy->opacity < 1.0,
        "Closing shrinks and fades before removal");
  advance(animations, 350);
  check(animations.activeCount() == 0 && wl_list_empty(&overlay->children),
        "Close releases its copied buffers when complete");
  check(liveBuffers == 0, "Close releases all retained backing buffers");

  window = wlr_scene_tree_create(normal);
  addBuffer(window, 120, 80);
  animations.setGeometry(window, initial, expanded, overlay);
  wlr_scene_node_destroy(&scene->tree.node);
  check(animations.activeCount() == 0,
        "Scene teardown cancels live transitions and snapshots without "
        "dangling pointers");
  check(liveBuffers == 0,
        "Scene teardown releases all retained backing buffers");
}
} // namespace LunaDash

int main(int argc, char **argv) {
  QCoreApplication application(argc, argv);
  LunaDash::testSceneAnimations();
  return 0;
}
