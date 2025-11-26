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

    // WaitSetDispatcher 생성 및 시작
    waitset_dispatcher_ = std::make_unique<async::WaitSetDispatcher>();
    waitset_dispatcher_->start();
}

/**
 * @brief 소멸자: 보유한 DDS 엔티티를 안전하게 정리합니다.
 * @details
 * - Reader/Writer를 우선 제거하고 Topic, Publisher/Subscriber, Participant
 *   순으로 해제하여 DDS 리소스 누수를 방지합니다.
 */
DdsManager::~DdsManager()
{
    if (waitset_dispatcher_) {
        waitset_dispatcher_->stop();
    }
    clear_entities();
}

void DdsManager::set_event_mode(EventMode mode) {
    std::lock_guard<std::mutex> lock(mutex_);
    // 안전성: 실행시점(mode)는 엔티티 생성 이전에만 설정 가능하도록 강제합니다.
    // 이미 어떤 엔티티라도 생성된 상태에서 모드를 바꾸면 내부 등록/해제와 충돌할 수 있으므로 예외로 처리합니다.
    bool has_entities = !participants_.empty() || !publishers_.empty() || !subscribers_.empty() || !writers_.empty() || !readers_.empty();
    if (has_entities) {
        LOG_ERR("DDS", "set_event_mode: cannot change event mode after entities have been created");
        throw std::logic_error("DdsManager::set_event_mode must be called before creating any DDS entities");
    }
    event_mode_ = mode;
    LOG_INF("DDS", "Event mode set to: %s", (mode == EventMode::Listener ? "Listener" : "WaitSet"));
}

// 헬퍼 함수 구현
void DdsManager::register_reader_event(std::shared_ptr<IReaderHolder> holder) {
    if (!holder) return;

    if (event_mode_ == EventMode::Listener) {
        // Listener 모드: Holder 자체 리스너 활성화 (데이터 수신 포함)
        holder->enable_listener_mode(true);
    } else {
        // WaitSet 모드: 리스너 끄고 Dispatcher에 등록
        holder->enable_listener_mode(false);
        
        if (waitset_dispatcher_) {
            // 1. 모니터링용 (StatusCondition) -> Monitor Thread
            waitset_dispatcher_->attach_monitor(holder.get());
            
            // 2. 데이터용 (ReadCondition) -> Data Thread
            waitset_dispatcher_->attach_data(holder.get());
        }
    }
}

void DdsManager::register_writer_event(std::shared_ptr<IWriterHolder> holder) {
    if (!holder) return;

    if (event_mode_ == EventMode::Listener) {
        holder->enable_listener_mode(true);
    } else {
        holder->enable_listener_mode(false);
        if (waitset_dispatcher_) {
            waitset_dispatcher_->attach_monitor(holder.get());
            // Writer는 Data Thread 등록 안 함
        }
    }
}

void DdsManager::unregister_reader_event(std::shared_ptr<IReaderHolder> holder) {
    if (!holder) return;
    
    // 모드 상관없이 모두 해제 시도 (안전하게)
    holder->enable_listener_mode(false);
    if (waitset_dispatcher_) {
        waitset_dispatcher_->detach_monitor(holder.get());
        waitset_dispatcher_->detach_data(holder.get());
    }
}

void DdsManager::unregister_writer_event(std::shared_ptr<IWriterHolder> holder) {
    if (!holder) return;

    holder->enable_listener_mode(false);
    if (waitset_dispatcher_) {
        waitset_dispatcher_->detach_monitor(holder.get());
    }
}

}  // namespace rtpdds
