/**
 * @file dds_manager.cpp
 * @brief DdsManager 구현 파일 (생성자/소멸자)
 */

#include <string>
#include "dds_manager.hpp"
#include "dds_type_registry.hpp"
#include "triad_log.hpp"

namespace rtpdds {

/**
 * @brief 생성자: 타입 레지스트리와 QoS 스토어를 초기화합니다.
 * @details
 * - IDL/JSON ↔ DDS 타입 매핑과 관련된 팩토리를 등록하기 위해
 *   init_dds_type_registry()를 호출합니다. 이후 writer/reader/topic 생성 시
 *   이 레지스트리를 참조합니다.
 * - QosStore는 QoS XML을 파싱하고 프로파일을 제공하는 역할을 합니다.
 *   생성자에 전달된 `qos_dir`에서 QoS 라이브러리를 탐색하여 초기화합니다.
 * - 디버그 목적의 팩토리 목록 로그를 출력하여 개발 시 가시성을 높입니다.
 */
DdsManager::DdsManager(const std::string& qos_dir)
{
    init_dds_type_registry();

    qos_store_ = std::make_unique<rtpdds::QosStore>(qos_dir);
    qos_store_->initialize();

    LOG_INF("DDS", "DdsManager initialized: topic_factories=%zu writer_factories=%zu reader_factories=%zu",
            topic_factories.size(), writer_factories.size(), reader_factories.size());

    // 상세 팩토리 목록은 디버그 레벨에서만 출력 (운영 로그 소음 방지)
    LOG_DBG("DDS", "Registered Topic Factories:");
    for (const auto& factory : topic_factories) {
        LOG_DBG("DDS", "  Topic Type: %s", factory.first.c_str());
    }

    LOG_DBG("DDS", "Registered Writer Factories:");
    for (const auto& factory : writer_factories) {
        LOG_DBG("DDS", "  Writer Type: %s", factory.first.c_str());
    }

    LOG_DBG("DDS", "Registered Reader Factories:");
    for (const auto& factory : reader_factories) {
        LOG_DBG("DDS", "  Reader Type: %s", factory.first.c_str());
    }
}

/**
 * @brief 소멸자: 보유한 DDS 엔티티를 안전하게 정리합니다.
 * @details
 * - Reader/Writer를 우선 제거하고 Topic, Publisher/Subscriber, Participant
 *   순으로 해제하여 DDS 리소스 누수를 방지합니다.
 */
DdsManager::~DdsManager()
{
    clear_entities();
}

}  // namespace rtpdds
