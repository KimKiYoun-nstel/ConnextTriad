#include "ngva_qos_loader.hpp"
#include <cstdio>

namespace ngva {

bool load_qos_from_xml(const std::string& path)
{
    DDSDomainParticipantFactory* factory = DDSDomainParticipantFactory::get_instance();
    if (!factory) return false;

    // 가장 안전한 루트: 환경변수 NDDS_QOS_PROFILES에도 추가되도록 가이드
    // 여기서는 Factory API로 로드(버전에 따라 load_profiles_from_xml 지원)
#if NDDS_MAJOR_VERSION >= 6
    DDS_ReturnCode_t rc = factory->load_profiles_from_xml(path.c_str());
    if (rc != DDS_RETCODE_OK) {
        std::fprintf(stderr, "[NGVA] load_profiles_from_xml failed: %s\n", path.c_str());
        return false;
    }
#else
    // 구버전 호환: 작업 디렉토리에 복사 or NDDS_QOS_PROFILES 사용 권장
    (void)path;
#endif
    return true;
}

DDSDomainParticipant* create_participant_with_profile(
    int domain_id,
    const char* library_name,
    const char* profile_name)
{
    DDS_DomainParticipantQos qos;
    DDSDomainParticipantFactory* factory = DDSDomainParticipantFactory::get_instance();
    if (!factory) return nullptr;

    if (factory->get_participant_qos_from_profile(qos, library_name, profile_name)
        != DDS_RETCODE_OK) {
        std::fprintf(stderr, "[NGVA] get_participant_qos_from_profile failed: %s::%s\n",
                     library_name, profile_name);
        return nullptr;
    }
    return factory->create_participant(
        domain_id, qos, NULL, DDS_STATUS_MASK_NONE);
}

} // namespace ngva
