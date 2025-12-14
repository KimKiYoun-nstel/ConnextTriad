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

// 이 함수 선언 변경 이유:
// 원래 파일 범위에서 "void rtpdds::InitRtiLoggerToTriad()" 형태로 명시적으로 네임스페이스를
// 사용했는데, GCC(및 일부 컴파일러)는 네임스페이스 블록 내부에서 함수 선언에 명시적 한정자를
// 허용하지 않습니다. 동작(로깅 브릿지 설정)은 변경하지 않으므로 플랫폼별 동작은 보존됩니다.
// Windows 빌드와의 호환성은 유지됩니다. (윈도우 전용 코드 변경은 없습니다.)
void InitRtiLoggerToTriad()
{
    auto& lg = Logger::instance();

    // 1) 기본 포맷
    lg.print_format(PrintFormat::MAXIMAL);

#ifdef _RTPDDS_DEBUG
    // Debug build: RTI logger verbose (모든 상태 메시지)
    lg.verbosity(Verbosity::status_local);
#else
    // Release build: RTI logger minimal (warning 이상만)
    lg.verbosity(Verbosity::warning);
#endif

    // 3) 범주별 미세 조정
    lg.verbosity_by_category(LogCategory::ALL_CATEGORIES, Verbosity::warning);
    lg.verbosity_by_category(LogCategory::user, Verbosity::warning);     // RTI Logger 사용자 로그 쓰는 경우만
    lg.verbosity_by_category(LogCategory::ENTITIES, Verbosity::status_local);  // 엔티티 라이프사이클(1회성 위주)
    lg.verbosity_by_category(LogCategory::DISCOVERY, Verbosity::warning);      // 디스커버리 경고만
    
    // COMMUNICATION 카테고리: warning으로 억제 (Heartbeat/ACK/NACK 등 주기적 로그 방지)
    // - Sample write/read는 Agent 자체 LOG_FLOW로 확인 (dds_type_registry.hpp)
    // - 통신 문제 발생 시에만 RTI 로그 출력
    lg.verbosity_by_category(LogCategory::COMMUNICATION, Verbosity::warning);
    
    lg.verbosity_by_category(LogCategory::API, Verbosity::warning);
    lg.verbosity_by_category(LogCategory::DATABASE, Verbosity::warning);
    lg.verbosity_by_category(LogCategory::PLATFORM, Verbosity::warning);
    lg.verbosity_by_category(LogCategory::SECURITY, Verbosity::warning);

    // 4) RTI→기존 로거 브릿지(그대로 유지)
    lg.output_handler([](const LogMessage& m) { forward_to_triad(m); });
}

void SetRtiLoggerVerbosity(Verbosity v) { Logger::instance().verbosity(v); }
}
