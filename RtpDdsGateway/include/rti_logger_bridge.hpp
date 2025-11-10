#pragma once
/**
 * @file rti_logger_bridge.hpp
 * @brief RTI Connext DDS Logger를 프로젝트의 triad_log로 연결하는 브릿지
 *
 * RTI 내부 로그를 triad_log 포맷으로 전달하기 위한 초기화/설정 API를 제공합니다.
 */
#include <rti/config/Logger.hpp>

namespace rtpdds {

/**
 * @brief RTI Logger를 triad_log 브릿지로 초기화합니다.
 * @details 애플리케이션 시작 시 1회 호출하며, 이후 RTI 로그가 triad_log를 통해 출력됩니다.
 */
void InitRtiLoggerToTriad();

/**
 * @brief RTI Logger의 Verbosity(로그 레벨)를 설정합니다.
 * @param v RTI Verbosity 값 (SILENT/EXCEPTION/WARNING/STATUS/ALL)
 * @note 필요시에만 호출하십시오. 기본값으로도 동작합니다.
 */
void SetRtiLoggerVerbosity(rti::config::Verbosity v);

} // namespace rtpdds
