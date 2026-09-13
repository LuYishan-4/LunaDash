#include <LuDash/tiling_core/TilingGeometry.h>
#include <LuDash/system_metrics/SystemMetrics.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
// Constant diagnostic format writes to stderr, not a caller-owned character buffer.
// NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
#define CHECK(condition) do { if (!(condition)) { fprintf(stderr, "C core check failed at line %d: %s\n", __LINE__, #condition); return 1; } } while (0)
int main(void) {
    LuDashRectangle rectangles[32];
    const LuDashRectangle area = {12, 52, 1200, 700};
    for (size_t count = 1; count <= 32; ++count) {
        CHECK(ludash_tile_rectangles(area, count, .56, 12, rectangles, 32) == count);
        for (size_t i = 0; i < count; ++i) {
            const LuDashRectangle a = rectangles[i];
            CHECK(a.width > 0 && a.height > 0 && a.x >= area.x && a.y >= area.y && a.x + a.width <= area.x + area.width && a.y + a.height <= area.y + area.height);
            for (size_t j = i + 1; j < count; ++j) {
                const LuDashRectangle b = rectangles[j];
                CHECK(a.x + a.width <= b.x || b.x + b.width <= a.x || a.y + a.height <= b.y || b.y + b.height <= a.y);
            }
        }
    }
    rectangles[0].x = 42;
    CHECK(!ludash_tile_rectangles(area, 3, .5, 12, rectangles, 2)); CHECK(rectangles[0].x == 42);
    CHECK(!ludash_tile_rectangles(area, 3, NAN, 12, rectangles, 32));
    CHECK(!ludash_tile_rectangles((LuDashRectangle){INT_MAX, 0, 2, 4}, 2, .5, 0, rectangles, 32));
    CHECK(!ludash_tile_rectangles((LuDashRectangle){0, 0, 1, 1}, 2, .5, 0, rectangles, 32));
    LuDashCpuCounters cpu = {0, 0};
    const char* cpu_text = "cpu 100 0 50 850 100 0 0 0 700 700\ncpu0 1 2 3 4\n";
    CHECK(ludash_parse_cpu(cpu_text, strlen(cpu_text), &cpu)); CHECK(cpu.total == 1100 && cpu.idle == 950);
    CHECK(ludash_cpu_percent(cpu, (LuDashCpuCounters){1300, 1000}) == 75);
    CHECK(ludash_cpu_percent(cpu, (LuDashCpuCounters){100, 0}) == 0);
    CHECK(!ludash_parse_cpu("cpu 1 -2 3 4", 12, &cpu)); CHECK(cpu.total == 1100);
    const char* overflow = "cpu 18446744073709551615 1 2 3";
    CHECK(!ludash_parse_cpu(overflow, strlen(overflow), &cpu));
    const char bounded[] = {'c','p','u',' ','1',' ','2',' ','3',' ','4'};
    CHECK(ludash_parse_cpu(bounded, sizeof(bounded), &cpu)); CHECK(cpu.total == 10);
    LuDashMemoryCounters memory = {0, 0};
    const char* memory_text = "MemTotal: 256000 kB\nMemFree: 100 kB\nMemAvailable: 64000 kB\n";
    CHECK(ludash_parse_memory(memory_text, strlen(memory_text), &memory));
    CHECK(ludash_memory_used(memory) == 192000 && ludash_memory_percent(memory) == 75);
    CHECK(!ludash_parse_memory("MemTotal: 2 MB\nMemAvailable: 1 kB", sizeof("MemTotal: 2 MB\nMemAvailable: 1 kB") - 1, &memory)); CHECK(memory.total_kib == 256000);
    CHECK(ludash_memory_used((LuDashMemoryCounters){1, 2}) == 0);
    CHECK(!ludash_parse_memory(NULL, 0, &memory)); CHECK(!ludash_parse_cpu(NULL, 0, &cpu));
    puts("C core tests passed: geometry, bounded parsers, overflow handling and counter resets.");
    return 0;
}
