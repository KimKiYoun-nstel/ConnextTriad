#pragma once

#include <dds/dds.hpp>
#include <thread>
#include <mutex>
#include <atomic>
#include <unordered_map>
#include "../dds_type_registry.hpp" // IDdsEventHandler 정의 포함

namespace rtpdds {
namespace async {

/**
 * @brief WaitSet 기반 이벤트 디스패처
 * 
 * 두 개의 독립된 스레드를 사용하여 데이터 처리와 상태 모니터링을 분리합니다.
 * - Data Thread: ReadCondition을 사용하여 데이터 수신 처리 (High Priority)
 * - Monitor Thread: StatusCondition을 사용하여 상태 변화 감지 (Low Priority)
 */
class WaitSetDispatcher {
public:
    WaitSetDispatcher();
    ~WaitSetDispatcher();

    /**
     * @brief 디스패처 스레드 시작
     */
    void start();

    /**
     * @brief 디스패처 스레드 중지
     */
    void stop();

    /**
     * @brief 모니터링 대상(Writer/Reader) 등록
     * @param handler 이벤트 핸들러 인터페이스
     */
    void attach_monitor(IDdsEventHandler* handler);

    /**
     * @brief 데이터 수신 대상(Reader) 등록
     * @param handler 이벤트 핸들러 인터페이스
     */
    void attach_data(IDdsEventHandler* handler);

    /**
     * @brief 모니터링 대상 제거
     */
    void detach_monitor(IDdsEventHandler* handler);

    /**
     * @brief 데이터 수신 대상 제거
     */
    void detach_data(IDdsEventHandler* handler);

private:
    void monitor_thread_loop();
    void data_thread_loop();

    std::atomic<bool> running_{false};
    
    // Monitor Thread 관련
    std::thread monitor_thread_;
    dds::core::cond::WaitSet monitor_waitset_;
    dds::core::cond::GuardCondition monitor_guard_; // Wakeup용
    std::mutex monitor_mutex_;
    std::unordered_map<dds::core::cond::Condition, IDdsEventHandler*> monitor_handlers_;

    // Data Thread 관련
    std::thread data_thread_;
    dds::core::cond::WaitSet data_waitset_;
    dds::core::cond::GuardCondition data_guard_; // Wakeup용
    std::mutex data_mutex_;
    std::unordered_map<dds::core::cond::Condition, IDdsEventHandler*> data_handlers_;
};

} // namespace async
} // namespace rtpdds
