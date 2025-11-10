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

    // 레벨 순서(숫자가 작을수록 더 상세한 로그)
    // 우선순위(가장 상세 -> 가장 심각): Debug(0) > Info(1) > Trace(2) > Warn(3) > Error(4)
    enum class Lvl { Debug = 0, Info = 1, Trace = 2, Warn = 3, Error = 4 };

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
        case Lvl::Trace:
            return "TRC";
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
        case Lvl::Trace:
            return "\033[36m"; // 청록 (trace 한 줄 로그 강조)
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
#if defined(_WIN32) || defined(_WIN64)
    localtime_s(&tm, &tt);
#else
    // POSIX: use localtime_r
    localtime_r(&tt, &tm);
#endif
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

// 운영용 한줄 요약 로그: 사용자의 정의에 따라 TRACE는 INFO보다 낮은 상세도(간결한 운영 레벨)
#define LOG_TRC(tag, fmt, ...)                                                 \
    ::triad::logf(::triad::Lvl::Trace, tag, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

// FLOW 전용 간편 매크로: 운영용 한줄 요약 로그를 찍을 때 사용합니다.
// 기존 코드에서 `LOG_TRC("FLOW", ...)`을 사용하는 대신 `LOG_FLOW(...)`로
// 표기하면 더 의도를 드러낼 수 있고, 향후 포맷 변경이 쉬워집니다.
#define LOG_FLOW(fmt, ...)                                                     \
    ::triad::logf(::triad::Lvl::Trace, "FLOW", __FILE__, __LINE__, fmt, ##__VA_ARGS__)
