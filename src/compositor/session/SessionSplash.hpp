#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>

struct wl_event_loop;
struct wlr_scene_tree;

namespace LunaDash {

// What a session shows while its shell starts, as KDE's splash does: the
// LunaDash mark over a dark screen and a light running along a thin bar. It
// fades out once finished.
class SessionSplash {
public:
  // `mark` is decoded on a thread; the bar runs meanwhile.
  SessionSplash(wlr_scene_tree *parent, wl_event_loop *loop,
                const std::filesystem::path &mark);
  ~SessionSplash();
  SessionSplash(const SessionSplash &) = delete;
  SessionSplash &operator=(const SessionSplash &) = delete;

  // Steps it to `nowNs` over a `width`×`height` screen; false once it has
  // faded out.
  bool advance(std::uint64_t nowNs, int width, int height);
  // The desktop is up: fade out.
  void finish();

private:
  struct Impl;
  std::unique_ptr<Impl> d;
};

} // namespace LunaDash
