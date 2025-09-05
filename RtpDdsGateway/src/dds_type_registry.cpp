/**
 * @file dds_type_registry.cpp
 * @brief DDS 엔티티(토픽/라이터/리더) 동적 생성 및 타입 안전성 제공, 타입별 팩토리/레지스트리 초기화 구현
 *
 * 각 함수별로 파라미터/리턴/비즈니스 처리에 대한 상세 주석을 추가.
 */
#include "dds_type_registry.hpp"
#include "StringMsg.hpp"
#include "AlarmMsg.hpp"
#include "Alarms_PSM.hpp"

namespace rtpdds {
// 레지스트리 컨테이너 정의: 토픽/라이터/리더 타입별 팩토리 함수 저장
std::unordered_map<std::string, TopicFactory> topic_factories;
std::unordered_map<std::string, WriterFactory> writer_factories;
std::unordered_map<std::string, ReaderFactory> reader_factories;

/**
 * @brief 타입별 팩토리/레지스트리 초기화 함수
 * 신규 타입 추가 시 이 함수에 register_dds_type<T>(type_name) 호출 필요
 */
void init_dds_type_registry() {
    register_dds_type<StringMsg>("StringMsg");
    register_dds_type<AlarmMsg>("AlarmMsg");
    register_dds_type<P_Alarms_PSM::C_Actual_Alarm>("P_Alarms_PSM::C_Actual_Alarm");
}
} // namespace rtpdds