#pragma once
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <ctime>
#include <functional>
#include <mutex>
#include <thread>
#include <cstring>
#include <vector>

namespace triad {

    enum class Lvl { Debug = 0, Info = 1, Warn = 2, Error = 3 };

    inline int &g_level() {
        static int v = (int)Lvl::Info;
        return v;
    }
    inline std::mutex &g_mx() {
        static std::mutex m;
        return m;
    }

    inline void set_level(Lvl l) {
        g_level() = (int)l;
    }

    inline const char *s(Lvl l) {
        switch (l) {
        case Lvl::Debug:
            return "DBG";
        case Lvl::Info:
            return "INF";
        case Lvl::Warn:
            return "WRN";
        default:
            return "ERR";
        }
    }

    inline const char* color_code(Lvl l) {
        switch (l) {
        case Lvl::Debug:
            return "\033[90m"; // 회색
        case Lvl::Info:
            return "\033[0m";  // 기본색
        case Lvl::Warn:
            return "\033[33m"; // 노란색
        case Lvl::Error:
        default:
            return "\033[31m"; // 빨간색
        }
    }

    // 파일명만 추출하는 함수
    inline const char* base_filename(const char* path) {
        if (!path) return "-";
        const char* slash = std::strrchr(path, '/');
        const char* backslash = std::strrchr(path, '\\');
        const char* fname = path;
        if (slash && backslash)
            fname = (slash > backslash) ? slash + 1 : backslash + 1;
        else if (slash)
            fname = slash + 1;
        else if (backslash)
            fname = backslash + 1;
        return fname;
    }

    // 파일명, 라인 추가
    inline void vlogf(Lvl lvl, const char *tag, const char *file, int line, const char *fmt, va_list ap) {
        if ((int)lvl < g_level())
            return;

    // Use a larger dynamic buffer to avoid truncating long log messages (e.g. large JSON dumps).
    // 16 KiB should be sufficient for most diagnostic logs; if larger logs are needed,
    // consider streaming to a file or chunking the output.
    std::vector<char> body(16 * 1024);
    std::vsnprintf(body.data(), body.size(), fmt, ap);

        auto now = std::chrono::system_clock::now();
        std::time_t tt = std::chrono::system_clock::to_time_t(now);
        std::tm tm;
        localtime_s(&tm, &tt);
        char ts[32];
        std::strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &tm);

        std::size_t tid =
            std::hash<std::thread::id>{}(std::this_thread::get_id());

    std::lock_guard<std::mutex> lk(g_mx());
    std::fprintf(stderr, "%s[%s] [%s] [tid:%zu] [%s] [%s:%d] %s\033[0m\n", color_code(lvl), ts, s(lvl), tid,
             tag ? tag : "-", base_filename(file), line, body.data());
    }

    inline void logf(Lvl lvl, const char *tag, const char *file, int line, const char *fmt, ...) {
        va_list ap;
        va_start(ap, fmt);
        vlogf(lvl, tag, file, line, fmt, ap);
        va_end(ap);
    }

} // namespace triad

#define LOG_DBG(tag, fmt, ...)                                                 \
    ::triad::logf(::triad::Lvl::Debug, tag, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_INF(tag, fmt, ...)                                                 \
    ::triad::logf(::triad::Lvl::Info, tag, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_WRN(tag, fmt, ...)                                                 \
    ::triad::logf(::triad::Lvl::Warn, tag, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_ERR(tag, fmt, ...)                                                 \
    ::triad::logf(::triad::Lvl::Error, tag, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
