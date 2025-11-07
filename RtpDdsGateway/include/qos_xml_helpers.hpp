/**
 * @file qos_xml_helpers.hpp
 * @brief QoS XML 변환/파싱/병합 유틸리티
 *
 * QosStore에서 사용하는 XML 관련 헬퍼들을 제공합니다. RTI QoS 객체를 XML로 변환하거나,
 * XML 파일/문자열에서 library::profile 정보를 추출하고, 특정 profile을 library XML에 병합하는 기능을 포함합니다.
 */

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

/**
 * @brief QosPack을 완전한 qos_profile XML로 변환 (압축 형식: 개행 없음)
 * @param profile_name 프로파일 이름
 * @param w DataWriter QoS
 * @param r DataReader QoS
 * @param t Topic QoS
 * @param base_name 상속할 base profile 이름 (선택적)
 * @return 완전한 <qos_profile> XML 문자열
 * @details
 * - 내부적으로 QoS 객체들을 읽어 적절한 XML 태그로 직렬화합니다.
 * - 반환 문자열은 로그/응답에 사용하기 쉽도록 개행이 제거된 단일 라인 형태입니다.
 */
std::string qos_pack_to_profile_xml(const std::string& profile_name, const dds::pub::qos::DataWriterQos& w,
                                     const dds::sub::qos::DataReaderQos& r, const dds::topic::qos::TopicQos& t,
                                     const std::string& base_name = "");

/**
 * @brief QoS XML 파일에서 모든 library::profile 이름 쌍을 추출
 * @param file_path QoS XML 파일 경로
 * @return (library_name, profile_name) 쌍의 벡터
 * @details
 * - 파일을 파싱하여 <qos_library name="..."> 내의 <qos_profile name="..."> 목록을 반환합니다.
 * - 실패 시 빈 벡터를 반환합니다(호출자는 빈 검사 필요).
 */
std::vector<std::pair<std::string, std::string>> parse_profiles_from_file(const std::string& file_path);

/**
 * @brief XML 콘텐츠 문자열에서 특정 library::profile의 XML 추출
 * @param content XML 전체 문자열
 * @param lib Library 이름
 * @param profile Profile 이름
 * @return 추출된 qos_profile XML (실패 시 빈 문자열)
 * @details
 * - content에서 해당 library::profile 블록을 찾아 반환합니다.
 * - 반환된 문자열은 원본 XML 조각(개행 포함)이며, 필요시 compress_xml로 정리할 수 있습니다.
 */
std::string extract_profile_xml_from_content(const std::string& content, const std::string& lib,
                                              const std::string& profile);

/**
 * @brief XML 문자열 압축 (개행, 탭, 불필요한 공백 제거)
 * @param xml 압축할 XML 문자열
 * @return 압축된 XML 문자열 (한 줄 형식)
 * @details
 * - 주로 로그/응답 바디에 포함시키기 위해 사용되며, 사람이 읽기 쉬운 형태가 아닐 수 있습니다.
 */
std::string compress_xml(const std::string& xml);

/**
 * @brief Profile XML을 특정 Library XML에 병합 (교체 또는 추가)
 * @param lib_xml 기존 Library XML 문자열(파일 전체 또는 단일 라이브러리 블록 포함 가능)
 * @param library_name 병합 대상 Library 이름
 * @param profile_name 병합할 Profile 이름 (name 속성은 이 값으로 강제 정규화됨)
 * @param profile_xml 병합할 Profile XML (qos_profile 태그 포함)
 * @return 병합된 Library XML (실패 시 빈 문자열)
 * @details
 * - 기존 profile이 존재하면 교체하고, 없으면 새로 추가합니다.
 * - 실패 시 빈 문자열을 반환합니다(호출자는 실패를 로그/에러로 처리해야 함).
 */
std::string merge_profile_into_library(const std::string& lib_xml,
                                       const std::string& library_name,
                                       const std::string& profile_name,
                                       const std::string& profile_xml);

}  // namespace qos_xml
}  // namespace rtpdds
