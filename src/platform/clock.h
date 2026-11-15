#pragma once

#include <windows.h>

#include <cstdint>

namespace m0 {

// High-resolution CPU clock (QueryPerformanceCounter). Ticks are only meaningful as differences.
inline int64_t clockNow() {
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    return t.QuadPart;
}

inline double clockMs(int64_t from, int64_t to) {
    static const double kTicksPerMs = [] {
        LARGE_INTEGER f;
        QueryPerformanceFrequency(&f);
        return static_cast<double>(f.QuadPart) / 1000.0;
    }();
    return static_cast<double>(to - from) / kTicksPerMs;
}

}  // namespace m0
