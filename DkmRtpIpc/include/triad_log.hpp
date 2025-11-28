#pragma once
#include <cstdarg>
#include <string>

namespace triad {

    // 레벨 순서(숫자가 작을수록 더 상세한 로그)
    // 우선순위(가장 상세 -> 가장 심각): Debug(0) > Info(1) > Trace(2) > Warn(3) > Error(4)
    enum class Lvl { Debug = 0, Info = 1, Trace = 2, Warn = 3, Error = 4 };

    // 로거 초기화 (앱 시작 시 호출)
    void init_logger(const std::string& log_dir, const std::string& filename, 
                     int max_size_mb, int max_files, bool console_out);
    
    // 로거 종료 (앱 종료 시 호출)
    void shutdown_logger();

    // 로그 레벨 설정
    void set_level(Lvl l);

    // 로그 출력 함수
    void logf(Lvl lvl, const char *tag, const char *file, int line, const char *fmt, ...);

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

// FLOW 전용 간편 매크로
#define LOG_FLOW(fmt, ...)                                                     \
    ::triad::logf(::triad::Lvl::Trace, "FLOW", __FILE__, __LINE__, fmt, ##__VA_ARGS__)

