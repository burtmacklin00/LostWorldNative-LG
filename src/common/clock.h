// Seconds from a fixed origin (QueryPerformanceCounter): the one clock for the
// game thread, the window and the UI, so their times can be compared
#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

inline double now_seconds() {
    static const double freq = [] {
        LARGE_INTEGER f;
        QueryPerformanceFrequency(&f);
        return double(f.QuadPart);
    }();
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    return double(now.QuadPart) / freq;
}
