#include "desktop/system/UpdateChecker.hpp"

#include <cassert>

int main() {
  using LunaDash::isNewerRelease;

  assert(isNewerRelease("1.2.0", "1.1.9"));
  assert(isNewerRelease("v1.2", "1.1.9"));
  assert(!isNewerRelease("1.2", "1.2.0"));
  assert(isNewerRelease("1.2.0", "1.2.0-rc1"));
  assert(isNewerRelease("1.2.0-rc.10", "1.2.0-rc.2"));
  assert(!isNewerRelease("not-a-version", "1.0.0"));
  assert(!isNewerRelease("1.0.0", "not-a-version"));
}
