#pragma once

#include <utility>

namespace LunaDash::Templates {

// Composition boundary for application-window animations. The compositor
// speaks lifecycle events; an animation owns renderer-specific implementation.
template <typename Animation> class WindowAnimationTemplate final {
public:
  WindowAnimationTemplate() = default;
  WindowAnimationTemplate(const WindowAnimationTemplate &) = delete;
  WindowAnimationTemplate &operator=(const WindowAnimationTemplate &) = delete;

  template <typename Profile> void configure(Profile &&profile) {
    animation_.configure(std::forward<Profile>(profile));
  }

  template <typename... Args> decltype(auto) open(Args &&...args) {
    return animation_.open(std::forward<Args>(args)...);
  }

  template <typename... Args> decltype(auto) close(Args &&...args) {
    return animation_.close(std::forward<Args>(args)...);
  }

  template <typename... Args> decltype(auto) relayout(Args &&...args) {
    return animation_.relayout(std::forward<Args>(args)...);
  }

  template <typename... Args> decltype(auto) focus(Args &&...args) {
    return animation_.focus(std::forward<Args>(args)...);
  }

  template <typename... Args> decltype(auto) cancel(Args &&...args) {
    return animation_.cancel(std::forward<Args>(args)...);
  }

  void clear() { animation_.clear(); }
  void advance() { animation_.advance(); }
  int activeCount() const { return animation_.activeCount(); }

  Animation &animation() { return animation_; }
  const Animation &animation() const { return animation_; }

private:
  Animation animation_;
};

} // namespace LunaDash::Templates
