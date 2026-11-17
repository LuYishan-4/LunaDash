#include "desktop/app/DesktopApplication.hpp"

int main(int argc, char **argv) {
  LunaDash::DesktopApplication application;
  return application.run(argc, argv);
}
