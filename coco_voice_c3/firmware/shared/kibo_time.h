// Milliseconds since boot (wraps after ~49 days; all comparisons use unsigned deltas).
#pragma once
#include <esp_timer.h>
#include <stdint.h>

namespace kibo {
inline uint32_t NowMs() { return (uint32_t)(esp_timer_get_time() / 1000); }
}  // namespace kibo
