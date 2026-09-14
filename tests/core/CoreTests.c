#include <LuDash/system_metrics/SystemMetrics.h>
#include <LuDash/tiling_core/TilingGeometry.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static void report_failure(int line, const char *condition) {
  // Constant diagnostic format writes to stderr, not a caller-owned buffer.
  // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
  fprintf(stderr, "C core check failed at line %d: %s\n", line, condition);
}
#define CHECK(condition)                                                       \
  do {                                                                         \
    if (!(condition)) {                                                        \
      report_failure(__LINE__, #condition);                                    \
      return 1;                                                                \
    }                                                                          \
  } while (0)
int main(void) {
  LuDashRectangle rectangles[32];
  const LuDashRectangle area = {12, 52, 1200, 700};
  for (size_t count = 1; count <= 32; ++count) {
    CHECK(ludash_tile_rectangles(area, count, .56, 12, rectangles, 32) ==
          count);
    for (size_t i = 0; i < count; ++i) {
      const LuDashRectangle a = rectangles[i];
      CHECK(a.width == 672 && a.height == area.height && a.y == area.y);
      if (i)
        CHECK(a.x == rectangles[i - 1].x + rectangles[i - 1].width + 12);
    }
  }
  CHECK(ludash_tile_rectangles(area, 2, .56, 12, rectangles, 32) == 2);
  const int first_width = rectangles[0].width;
  CHECK(ludash_tile_rectangles(area, 3, .56, 12, rectangles, 32) == 3);
  CHECK(rectangles[0].width == first_width &&
        rectangles[1].width == first_width);

  const int widths[] = {300, 480, 220};
  CHECK(ludash_layout_columns(area, widths, 3, 10, 175, rectangles, 32) == 3);
  CHECK(rectangles[0].x == area.x - 175 && rectangles[0].width == 300);
  CHECK(rectangles[1].x == rectangles[0].x + 310 && rectangles[1].width == 480);
  rectangles[0].x = 42;
  CHECK(!ludash_layout_columns(area, widths, 3, 10, 0, rectangles, 2));
  CHECK(rectangles[0].x == 42);
  const int overflowing_widths[] = {INT_MAX, 1};
  CHECK(!ludash_layout_columns(area, overflowing_widths, 2, 1, 0, rectangles,
                               32));
  CHECK(rectangles[0].x == 42);
  CHECK(!ludash_layout_columns(area, widths, 3, -1, 0, rectangles, 32));

  CHECK(ludash_layout_column_windows((LuDashRectangle){10, 20, 400, 603}, 4, 10,
                                     rectangles, 32) == 4);
  CHECK(rectangles[0].height == 143);
  CHECK(rectangles[1].height == 143);
  CHECK(rectangles[2].height == 143);
  CHECK(rectangles[3].height == 144);
  for (size_t i = 1; i < 4; ++i) {
    CHECK(rectangles[i].x == 10 && rectangles[i].width == 400);
    CHECK(rectangles[i].y ==
          rectangles[i - 1].y + rectangles[i - 1].height + 10);
  }
  CHECK(ludash_layout_column_windows((LuDashRectangle){10, 20, 400, 603}, 3, 10,
                                     rectangles, 32) == 3);
  CHECK(rectangles[0].height == 145);
  CHECK(rectangles[1].height == 146);
  CHECK(rectangles[2].height == 292);
  rectangles[0].x = 42;
  CHECK(!ludash_layout_column_windows(area, 4, 10, rectangles, 3));
  CHECK(!ludash_layout_column_windows(area, 5, 10, rectangles, 32));
  CHECK(rectangles[0].x == 42);
  CHECK(!ludash_layout_column_windows((LuDashRectangle){0, 0, 10, 3}, 4, 1,
                                      rectangles, 32));
  CHECK(rectangles[0].x == 42);
  CHECK(!ludash_tile_rectangles(area, 3, NAN, 12, rectangles, 32));
  CHECK(!ludash_tile_rectangles((LuDashRectangle){INT_MAX, 0, 2, 4}, 2, .5, 0,
                                rectangles, 32));
  LuDashCpuCounters cpu = {0, 0};
  const char *cpu_text = "cpu 100 0 50 850 100 0 0 0 700 700\ncpu0 1 2 3 4\n";
  CHECK(ludash_parse_cpu(cpu_text, strlen(cpu_text), &cpu));
  CHECK(cpu.total == 1100 && cpu.idle == 950);
  CHECK(ludash_cpu_percent(cpu, (LuDashCpuCounters){1300, 1000}) == 75);
  CHECK(ludash_cpu_percent(cpu, (LuDashCpuCounters){100, 0}) == 0);
  CHECK(!ludash_parse_cpu("cpu 1 -2 3 4", 12, &cpu));
  CHECK(cpu.total == 1100);
  const char *overflow = "cpu 18446744073709551615 1 2 3";
  CHECK(!ludash_parse_cpu(overflow, strlen(overflow), &cpu));
  const char bounded[] = {'c', 'p', 'u', ' ', '1', ' ',
                          '2', ' ', '3', ' ', '4'};
  CHECK(ludash_parse_cpu(bounded, sizeof(bounded), &cpu));
  CHECK(cpu.total == 10);
  LuDashMemoryCounters memory = {0, 0};
  const char *memory_text =
      "MemTotal: 256000 kB\nMemFree: 100 kB\nMemAvailable: 64000 kB\n";
  CHECK(ludash_parse_memory(memory_text, strlen(memory_text), &memory));
  CHECK(ludash_memory_used(memory) == 192000 &&
        ludash_memory_percent(memory) == 75);
  CHECK(!ludash_parse_memory("MemTotal: 2 MB\nMemAvailable: 1 kB",
                             sizeof("MemTotal: 2 MB\nMemAvailable: 1 kB") - 1,
                             &memory));
  CHECK(memory.total_kib == 256000);
  CHECK(ludash_memory_used((LuDashMemoryCounters){1, 2}) == 0);
  CHECK(!ludash_parse_memory(NULL, 0, &memory));
  CHECK(!ludash_parse_cpu(NULL, 0, &cpu));
  puts("C core tests passed: geometry, bounded parsers, overflow handling and "
       "counter resets.");
  return 0;
}
