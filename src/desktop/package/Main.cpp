#include "desktop/package/PackageManagerApplication.hpp"

int main(int argc, char **argv) {
  LunaDash::PackageManagerApplication application;
  return application.run(argc, argv);
}
