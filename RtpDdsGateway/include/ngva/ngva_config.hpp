#pragma once
// NGVA 통합 전역 설정 (런타임/환경에 따라 오버라이드 가능)

#ifndef NGVA_ENABLED
#define NGVA_ENABLED 1
#endif

#include <string>

namespace ngva {

struct Config {
    // RMDP QoS XML 경로 (기본값)
    std::string qos_xml = "ngva/qos/ngva_qos_profiles.xml";
    // 기본 QoS 프로파일 (RMDP 문서에 명시된 library::profile 사용)
    std::string participant_profile = "NGVA_Library::Participant_Default";
    std::string publisher_profile    = "NGVA_Library::Publisher_Default";
    std::string subscriber_profile   = "NGVA_Library::Subscriber_Default";
    std::string writer_profile       = "NGVA_Library::Writer_Default";
    std::string reader_profile       = "NGVA_Library::Reader_Default";

    // Topic 이름(예시) — 실제 RMDP Topic Names IDL과 맞추세요
    std::string arbitration_topic = "NGVA.Arbitration";
    std::string registry_topic    = "NGVA.Registry";

    // 도메인/리더 타임아웃 등
    int domain_id = 0;
    int waitset_timeout_ms = 100;

    static Config from_env();
};

} // namespace ngva
