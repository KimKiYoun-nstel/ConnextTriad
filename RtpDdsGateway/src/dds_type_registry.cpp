#include "dds_type_registry.hpp"
#include "StringMsg.hpp"
#include "AlarmMsg.hpp"
#include "Alarms_PSM.hpp"

namespace rtpdds {
// 레지스트리 컨테이너 정의
std::unordered_map<std::string, TopicFactory> topic_factories;
std::unordered_map<std::string, WriterFactory> writer_factories;
std::unordered_map<std::string, ReaderFactory> reader_factories;

// 타입 등록 함수
void init_dds_type_registry() {
    register_dds_type<StringMsg>("StringMsg");
    register_dds_type<AlarmMsg>("AlarmMsg");
    register_dds_type<P_Alarms_PSM::C_Actual_Alarm>("P_Alarms_PSM::C_Actual_Alarm");
}
}