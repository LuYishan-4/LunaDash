#include "desktop/input/InputSettings.hpp"

#include <iostream>

int main() {
  if (LunaDash::keyboardLayoutPreference() != "us" ||
      LunaDash::keyboardLayoutPreference("us") != "us" ||
      LunaDash::keyboardLayoutPreference("invalid-layout") != "us" ||
      LunaDash::keyboardRepeatRate() != 25 ||
      LunaDash::keyboardRepeatDelay() != 600) {
    std::cerr << "Keyboard settings validation contract failed.\n";
    return 1;
  }
  std::cout << "Native keyboard layout and repeat settings passed.\n";
  return 0;
}
