/**
 * @file dds_manager_internal.hpp
 * @brief DdsManager 내부 공용 유틸리티(로깅 헬퍼) - 내부 전용 헤더
 */
#pragma once
#include <string>
#include <cstddef>
#include "triad_log.hpp"

namespace rtpdds { namespace internal {

// 문자열 길이 제한 로깅용 프리뷰
/**
 * @brief 긴 문자열을 로그 용도로 안전하게 자릅니다.
 * @param s 원본 문자열
 * @param max_len 유지할 최대 길이(기본 200)
 * @return 잘린 문자열(길이 초과 시 ...(len=N) 접미사 추가)
 * @details
 * - 주로 디버그/정보 로그에서 지나치게 긴 페이로드를 줄여 로그 파일 크기와 가독성을 개선합니다.
 */
inline std::string truncate_for_log(const std::string& s, std::size_t max_len = 200) {
    if (s.size() <= max_len) return s;
    return s.substr(0, max_len) + "...(len=" + std::to_string(s.size()) + ")";
}

// 공개 API 진입 로깅을 일관되게 기록
/**
 * @brief 공개 API 진입 로그를 일관되게 남깁니다.
 * @param fn 함수명(문자열 리터럴 권장)
 * @param params 함수 파라미터 요약 문자열
 * @details
 * - 운영 환경의 로그 소음을 줄이기 위해 기본 로그 레벨은 DEBUG로 기록합니다.
 * - 호출부는 보안상 민감 정보가 포함되지 않도록 params를 구성해야 합니다.
 */
inline void log_entry(const char* fn, const std::string& params) {
    // 기본 동작은 디버그 레벨로 함수 진입을 기록하여 운영 환경 로그 소음을 줄임
    LOG_DBG("DDS", "[ENTRY] %s %s", fn, params.c_str());
}

}} // namespace rtpdds::internal
