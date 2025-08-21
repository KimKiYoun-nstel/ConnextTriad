#pragma once
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <ctime>
#include <functional>
#include <mutex>
#include <thread>

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

    inline void vlogf(Lvl lvl, const char *tag, const char *fmt, va_list ap) {
        if ((int)lvl < g_level())
            return;

        char body[2048];
        std::vsnprintf(body, sizeof(body), fmt, ap);

        auto now = std::chrono::system_clock::now();
        std::time_t tt = std::chrono::system_clock::to_time_t(now);
        std::tm tm;
        localtime_s(&tm, &tt);
        char ts[32];
        std::strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &tm);

        std::size_t tid =
            std::hash<std::thread::id>{}(std::this_thread::get_id());

        std::lock_guard<std::mutex> lk(g_mx());
        std::fprintf(stderr, "[%s] [%s] [tid:%zu] [%s] %s\n", ts, s(lvl), tid,
                     tag ? tag : "-", body);
    }

    inline void logf(Lvl lvl, const char *tag, const char *fmt, ...) {
        va_list ap;
        va_start(ap, fmt);
        vlogf(lvl, tag, fmt, ap);
        va_end(ap);
    }

} // namespace triad

#define LOG_DBG(tag, fmt, ...)                                                 \
    ::triad::logf(::triad::Lvl::Debug, tag, fmt, ##__VA_ARGS__)
#define LOG_INF(tag, fmt, ...)                                                 \
    ::triad::logf(::triad::Lvl::Info, tag, fmt, ##__VA_ARGS__)
#define LOG_WRN(tag, fmt, ...)                                                 \
    ::triad::logf(::triad::Lvl::Warn, tag, fmt, ##__VA_ARGS__)
#define LOG_ERR(tag, fmt, ...)                                                 \
    ::triad::logf(::triad::Lvl::Error, tag, fmt, ##__VA_ARGS__)
