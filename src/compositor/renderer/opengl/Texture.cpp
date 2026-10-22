#include "compositor/renderer/opengl/Texture.hpp"

namespace LunaDash {

Texture::~Texture() { reset(); }

bool Texture::resize(LuDashGLDispatch &gl, const QSize &size) {
  if (size.width() <= 0 || size.height() <= 0 || size.width() > 32768 ||
      size.height() > 32768)
    return false;
  if (gl_ == &gl && texture_ && size_ == size)
    return true;

  reset();
  gl_ = &gl;
  gl.GenTextures(1, &texture_);
  if (!texture_) {
    gl_ = nullptr;
    return false;
  }

  gl.BindTexture(GL_TEXTURE_2D, texture_);
  gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  gl.TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, size.width(), size.height(), 0,
                GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
  size_ = size;
  return true;
}

void Texture::reset() {
  if (gl_ && texture_)
    gl_->DeleteTextures(1, &texture_);
  texture_ = 0;
  size_ = {};
  gl_ = nullptr;
}

GLuint Texture::texture() const { return texture_; }
QSize Texture::size() const { return size_; }
bool Texture::valid() const { return texture_ != 0 && !size_.isEmpty(); }

} // namespace LunaDash
