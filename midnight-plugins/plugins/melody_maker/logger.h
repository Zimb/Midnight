#pragma once
// =============================================================================
// logger.h — Thread-safe file logger for Midnight Melody Maker (debug only)
//
// Usage:
//   MM_LOG("process() triggered, beatPos=%.3f", beatPos);
//   MM_LOG_ONCE("sfReady is false");   // logs only the first time this line fires
//
// Output: %TEMP%\midnight_debug.log
// Enable: define MM_DEBUG_LOG before including this header (or in CMakeLists.txt)
// Disable for release: simply don't define MM_DEBUG_LOG (zero-cost, no overhead)
// =============================================================================

#ifdef MM_DEBUG_LOG

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <string>
#include <atomic>

namespace mm_log {

// One global CRITICAL_SECTION protects concurrent writes from the audio
// thread and the GUI thread.
inline CRITICAL_SECTION& cs() {
    static CRITICAL_SECTION g_cs;
    static bool g_init = []{ InitializeCriticalSection(&g_cs); return true; }();
    (void)g_init;
    return g_cs;
}

inline FILE*& fp() {
    static FILE* g_fp = nullptr;
    return g_fp;
}

inline void open() {
    if (fp()) return;
    wchar_t tmp[MAX_PATH];
    GetTempPathW(MAX_PATH, tmp);
    std::wstring path = std::wstring(tmp) + L"midnight_debug.log";
    char path8[MAX_PATH * 4] = {};
    WideCharToMultiByte(CP_ACP, 0, path.c_str(), -1, path8, sizeof(path8), nullptr, nullptr);
    fp() = fopen(path8, "w");
    if (fp()) fprintf(fp(), "=== Midnight Debug Log ===\n\n");
}

inline void write(const char* file, int line, const char* fmt, ...) {
    EnterCriticalSection(&cs());
    open();
    if (fp()) {
        // Timestamp in milliseconds using QueryPerformanceCounter
        static LARGE_INTEGER freq = []{ LARGE_INTEGER f; QueryPerformanceFrequency(&f); return f; }();
        LARGE_INTEGER now; QueryPerformanceCounter(&now);
        double ms = (double)now.QuadPart / freq.QuadPart * 1000.0;

        // Extract just the filename (strip path)
        const char* slash = file;
        for (const char* p = file; *p; ++p) if (*p == '\\' || *p == '/') slash = p + 1;

        fprintf(fp(), "[%10.1f ms] %s:%d  ", ms, slash, line);
        va_list va; va_start(va, fmt); vfprintf(fp(), fmt, va); va_end(va);
        fprintf(fp(), "\n");
        fflush(fp());
    }
    LeaveCriticalSection(&cs());
}

} // namespace mm_log

// --------------- Public macros -----------------------------------------------

#define MM_LOG(fmt, ...) \
    mm_log::write(__FILE__, __LINE__, fmt, ##__VA_ARGS__)

// Logs only the first time a given call site is reached.
#define MM_LOG_ONCE(fmt, ...) \
    do { \
        static std::atomic<bool> _done{false}; \
        if (!_done.exchange(true, std::memory_order_relaxed)) \
            mm_log::write(__FILE__, __LINE__, "ONCE: " fmt, ##__VA_ARGS__); \
    } while(0)

// Logs only when the value of `expr` changes between calls at this call site.
#define MM_LOG_CHANGE(label, expr) \
    do { \
        static auto _prev = (expr); \
        auto _cur = (expr); \
        if (_cur != _prev) { \
            mm_log::write(__FILE__, __LINE__, "CHANGE %s: %s -> %s", \
                label, std::to_string((int)_prev).c_str(), std::to_string((int)_cur).c_str()); \
            _prev = _cur; \
        } \
    } while(0)

// Throttled log: fires at most every N invocations (cheap audio-thread log).
#define MM_LOG_EVERY(n, fmt, ...) \
    do { \
        static std::atomic<int> _cnt{0}; \
        if (_cnt.fetch_add(1, std::memory_order_relaxed) % (n) == 0) \
            mm_log::write(__FILE__, __LINE__, "[every %d] " fmt, (n), ##__VA_ARGS__); \
    } while(0)

#else // MM_DEBUG_LOG not defined — all macros vanish completely

#define MM_LOG(fmt, ...)          do {} while(0)
#define MM_LOG_ONCE(fmt, ...)     do {} while(0)
#define MM_LOG_CHANGE(label, expr) do {} while(0)
#define MM_LOG_EVERY(n, fmt, ...) do {} while(0)

#endif // MM_DEBUG_LOG
