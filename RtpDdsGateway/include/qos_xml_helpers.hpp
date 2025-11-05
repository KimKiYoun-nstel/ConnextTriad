// Copyright (c) 2025 NSTEL Inc.
// QoS XML 변환 및 처리 유틸리티 함수들
// QosStore에서 사용하는 XML 관련 헬퍼 함수들을 별도 파일로 분리

#pragma once

#include <dds/core/Duration.hpp>
#include <dds/core/policy/CorePolicy.hpp>
#include <dds/pub/qos/DataWriterQos.hpp>
#include <dds/sub/qos/DataReaderQos.hpp>
#include <dds/topic/qos/TopicQos.hpp>
#include <string>
#include <utility>
#include <vector>

namespace rtpdds
{
namespace qos_xml
{

/// QosPack을 완전한 qos_profile XML로 변환 (압축 형식: 개행 없음)
/// @param profile_name 프로파일 이름
/// @param w DataWriter QoS
/// @param r DataReader QoS
/// @param t Topic QoS
/// @param base_name 상속할 base profile 이름 (선택적)
/// @return 완전한 <qos_profile> XML 문자열
std::string qos_pack_to_profile_xml(const std::string& profile_name, const dds::pub::qos::DataWriterQos& w,
                                     const dds::sub::qos::DataReaderQos& r, const dds::topic::qos::TopicQos& t,
                                     const std::string& base_name = "");

/// QoS XML 파일에서 모든 library::profile 이름 쌍을 추출
/// @param file_path QoS XML 파일 경로
/// @return (library_name, profile_name) 쌍의 벡터
std::vector<std::pair<std::string, std::string>> parse_profiles_from_file(const std::string& file_path);

/// XML 콘텐츠 문자열에서 특정 library::profile의 XML 추출
/// @param content XML 전체 문자열
/// @param lib Library 이름
/// @param profile Profile 이름
/// @return 추출된 qos_profile XML (실패 시 빈 문자열)
std::string extract_profile_xml_from_content(const std::string& content, const std::string& lib,
                                              const std::string& profile);

/// XML 문자열 압축 (개행, 탭, 불필요한 공백 제거)
/// @param xml 압축할 XML 문자열
/// @return 압축된 XML 문자열 (한 줄 형식)
std::string compress_xml(const std::string& xml);

/// Profile XML을 특정 Library XML에 병합 (교체 또는 추가)
/// @param lib_xml 기존 Library XML 문자열(파일 전체 또는 단일 라이브러리 블록 포함 가능)
/// @param library_name 병합 대상 Library 이름
/// @param profile_name 병합할 Profile 이름 (name 속성은 이 값으로 강제 정규화됨)
/// @param profile_xml 병합할 Profile XML (qos_profile 태그 포함)
/// @return 병합된 Library XML (실패 시 빈 문자열)
std::string merge_profile_into_library(const std::string& lib_xml,
                                       const std::string& library_name,
                                       const std::string& profile_name,
                                       const std::string& profile_xml);

}  // namespace qos_xml
}  // namespace rtpdds
