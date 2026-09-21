#pragma once

#include <utility>

namespace LunaDash::Templates {

// Composition boundary for application-window animations. The compositor
// speaks lifecycle events; a backend owns renderer-specific implementation.
template <typename Backend> class WindowAnimationTemplate final {
public:
  WindowAnimationTemplate() = default;
  WindowAnimationTemplate(const WindowAnimationTemplate &) = delete;
  WindowAnimationTemplate &operator=(const WindowAnimationTemplate &) = delete;

  template <typename Profile> void configure(Profile &&profile) {
    backend_.configure(std::forward<Profile>(profile));
  }

  template <typename... Args> decltype(auto) open(Args &&...args) {
    return backend_.open(std::forward<Args>(args)...);
  }

  template <typename... Args> decltype(auto) close(Args &&...args) {
    return backend_.close(std::forward<Args>(args)...);
  }

  template <typename... Args> decltype(auto) relayout(Args &&...args) {
    return backend_.relayout(std::forward<Args>(args)...);
  }

  template <typename... Args> decltype(auto) focus(Args &&...args) {
    return backend_.focus(std::forward<Args>(args)...);
  }

  template <typename... Args> decltype(auto) cancel(Args &&...args) {
    return backend_.cancel(std::forward<Args>(args)...);
  }

  void clear() { backend_.clear(); }
  void advance() { backend_.advance(); }
  int activeCount() const { return backend_.activeCount(); }

  Backend &backend() { return backend_; }
  const Backend &backend() const { return backend_; }

private:
  Backend backend_;
};

} // namespace LunaDash::Templates
