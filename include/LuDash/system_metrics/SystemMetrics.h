#pragma once
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
namespace LuDash {
extern "C" {
#endif
typedef struct LuDashCpuCounters { uint64_t total, idle; } LuDashCpuCounters;
typedef struct LuDashMemoryCounters { uint64_t total_kib, available_kib; } LuDashMemoryCounters;
int ludash_parse_cpu(const char* text, size_t length, LuDashCpuCounters* output);
int ludash_parse_memory(const char* text, size_t length, LuDashMemoryCounters* output);
int ludash_cpu_percent(LuDashCpuCounters previous, LuDashCpuCounters current);
uint64_t ludash_memory_used(LuDashMemoryCounters memory);
int ludash_memory_percent(LuDashMemoryCounters memory);
#ifdef __cplusplus
}
}
#endif
