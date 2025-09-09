#include "rti_logger_bridge.hpp"
#include "triad_log.hpp"          // LOG_ERR/LOG_WRN/LOG_INF/LOG_DBG
using namespace rti::config;

namespace rtpdds
{
static inline void forward_to_triad(const rti::config::LogMessage& m)
{
    const LogLevel lvl = m.level;
    if (lvl == LogLevel::FATAL_ERROR || lvl == LogLevel::EXCEPTION) {
        LOG_ERR("RTI", "%s", m.text);
    } else if (lvl == LogLevel::WARNING) {
        LOG_WRN("RTI", "%s", m.text);
    } else if (lvl == LogLevel::STATUS_LOCAL || lvl == LogLevel::STATUS_REMOTE ) {
        LOG_INF("RTI", "%s", m.text);
    } else { // || lvl == LogLevel::STATUS_ALL
        LOG_DBG("RTI", "%s", m.text);
    }
}

void rtpdds::InitRtiLoggerToTriad()
{
    auto& lg = Logger::instance();

    // 1) 기본 포맷
    lg.print_format(PrintFormat::MAXIMAL);

    // 2) 전역은 WARNING 이하로
    lg.verbosity(Verbosity::status_local);  // default는 exception

    // 3) 범주별 미세 조정
    lg.verbosity_by_category(LogCategory::ALL_CATEGORIES, Verbosity::warning);
    lg.verbosity_by_category(LogCategory::user, Verbosity::warning);     // RTI Logger 사용자 로그 쓰는 경우만
    lg.verbosity_by_category(LogCategory::ENTITIES, Verbosity::status_local);  // 엔티티 라이프사이클(1회성 위주)
    lg.verbosity_by_category(LogCategory::DISCOVERY, Verbosity::warning);      // 디스커버리 경고만
    lg.verbosity_by_category(LogCategory::COMMUNICATION, Verbosity::exception);  // 통신 상세 억제
    lg.verbosity_by_category(LogCategory::API, Verbosity::warning);
    lg.verbosity_by_category(LogCategory::DATABASE, Verbosity::warning);
    lg.verbosity_by_category(LogCategory::PLATFORM, Verbosity::warning);
    lg.verbosity_by_category(LogCategory::SECURITY, Verbosity::warning);

    // 4) RTI→기존 로거 브릿지(그대로 유지)
    lg.output_handler([](const LogMessage& m) { forward_to_triad(m); });
}

void SetRtiLoggerVerbosity(Verbosity v) { Logger::instance().verbosity(v); }
}
