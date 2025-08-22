#pragma once
#include <string>
#include "ndds/ndds_cpp.h"

namespace ngva {

bool load_qos_from_xml(const std::string& path);

// 프로파일이 로드되어 있다고 가정하고 profile 기반 Participant 생성
DDSDomainParticipant* create_participant_with_profile(
    int domain_id,
    const char* library_name,
    const char* profile_name);

} // namespace ngva
