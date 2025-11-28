#include "../../include/async/waitset_dispatcher.hpp"
#include "../../DkmRtpIpc/include/triad_log.hpp"

namespace rtpdds {
namespace async {

WaitSetDispatcher::WaitSetDispatcher() {
    // GuardCondition을 미리 attach하여 언제든 깨울 수 있게 함
    monitor_waitset_.attach_condition(monitor_guard_);
    data_waitset_.attach_condition(data_guard_);
}

WaitSetDispatcher::~WaitSetDispatcher() {
    stop();
}

void WaitSetDispatcher::start() {
    if (running_) return;
    running_ = true;

    monitor_thread_ = std::thread(&WaitSetDispatcher::monitor_thread_loop, this);
    data_thread_ = std::thread(&WaitSetDispatcher::data_thread_loop, this);
    
    LOG_INF("WaitSetDispatcher", "Started monitor and data threads");
}

void WaitSetDispatcher::stop() {
    if (!running_) return;
    running_ = false;

    // 스레드 깨우기
    monitor_guard_.trigger_value(true);
    data_guard_.trigger_value(true);

    if (monitor_thread_.joinable()) monitor_thread_.join();
    if (data_thread_.joinable()) data_thread_.join();

    LOG_INF("WaitSetDispatcher", "Stopped threads");
}

void WaitSetDispatcher::detach_all()
{
    // 호출 시점은 스레드가 중지된 후여야 합니다.
    try {
        {
            std::lock_guard<std::mutex> lock(monitor_mutex_);
            for (auto &kv : monitor_handlers_) {
                try {
                    monitor_waitset_.detach_condition(kv.first);
                } catch (...) {}
            }
            monitor_handlers_.clear();
        }

        {
            std::lock_guard<std::mutex> lock(data_mutex_);
            for (auto &kv : data_handlers_) {
                try {
                    data_waitset_.detach_condition(kv.first);
                } catch (...) {}
            }
            data_handlers_.clear();
        }
    } catch (const std::exception& e) {
        LOG_ERR("WaitSetDispatcher", "detach_all exception: %s", e.what());
    }
}

void WaitSetDispatcher::attach_monitor(IDdsEventHandler* handler) {
    if (!handler) return;
    std::lock_guard<std::mutex> lock(monitor_mutex_);
    
    try {
        auto cond = handler->get_status_condition();
        // 데이터 수신(data_available)을 제외한 상태만 활성화
        // (data_available은 Data Thread에서 처리하거나, Listener 모드에서 처리)
        cond.enabled_statuses(
            dds::core::status::StatusMask::all() & ~dds::core::status::StatusMask::data_available()
        );
        
        monitor_waitset_.attach_condition(cond);
        monitor_handlers_[cond] = handler;
        LOG_DBG("WaitSetDispatcher", "Attached monitor handler");
    } catch (const std::exception& e) {
        LOG_ERR("WaitSetDispatcher", "Failed to attach monitor: %s", e.what());
    }
}

void WaitSetDispatcher::attach_data(IDdsEventHandler* handler) {
    if (!handler) return;
    std::lock_guard<std::mutex> lock(data_mutex_);

    try {
        auto cond = handler->get_read_condition();
        // Writer는 null condition을 반환하므로 체크
        if (cond == dds::core::null) return;

        data_waitset_.attach_condition(cond);
        data_handlers_[cond] = handler;
        LOG_DBG("WaitSetDispatcher", "Attached data handler");
    } catch (const std::exception& e) {
        LOG_ERR("WaitSetDispatcher", "Failed to attach data: %s", e.what());
    }
}

void WaitSetDispatcher::detach_monitor(IDdsEventHandler* handler) {
    if (!handler) return;
    std::lock_guard<std::mutex> lock(monitor_mutex_);

    try {
        auto cond = handler->get_status_condition();
        monitor_waitset_.detach_condition(cond);
        monitor_handlers_.erase(cond);
    } catch (...) {}
}

void WaitSetDispatcher::detach_data(IDdsEventHandler* handler) {
    if (!handler) return;
    std::lock_guard<std::mutex> lock(data_mutex_);

    try {
        auto cond = handler->get_read_condition();
        if (cond == dds::core::null) return;
        
        data_waitset_.detach_condition(cond);
        data_handlers_.erase(cond);
    } catch (...) {}
}

void WaitSetDispatcher::monitor_thread_loop() {
    while (running_) {
        try {
            // 1초 타임아웃으로 대기 (RTI Modern C++ API: wait() returns ConditionSeq)
            dds::core::cond::WaitSet::ConditionSeq active_conditions = 
                monitor_waitset_.wait(dds::core::Duration(1, 0));

            if (!running_) break;

            std::lock_guard<std::mutex> lock(monitor_mutex_);
            for (auto& cond : active_conditions) {
                if (cond == monitor_guard_) {
                    monitor_guard_.trigger_value(false);
                    continue;
                }

                auto it = monitor_handlers_.find(cond);
                if (it != monitor_handlers_.end()) {
                    // StatusCondition에서 변경된 상태 마스크 추출
                    auto status_cond = dds::core::polymorphic_cast<dds::core::cond::StatusCondition>(cond);
                    auto entity = status_cond.entity();
                    auto mask = entity.status_changes();
                    
                    it->second->process_status(mask);
                }
            }
        } catch (const dds::core::TimeoutError&) {
            // 타임아웃은 정상 동작
        } catch (const std::exception& e) {
            LOG_ERR("WaitSetDispatcher", "Monitor thread exception: %s", e.what());
        }
    }
}

void WaitSetDispatcher::data_thread_loop() {
    while (running_) {
        try {
            dds::core::cond::WaitSet::ConditionSeq active_conditions = 
                data_waitset_.wait(dds::core::Duration(1, 0));

            if (!running_) break;

            std::lock_guard<std::mutex> lock(data_mutex_);
            for (auto& cond : active_conditions) {
                if (cond == data_guard_) {
                    data_guard_.trigger_value(false);
                    continue;
                }

                auto it = data_handlers_.find(cond);
                if (it != data_handlers_.end()) {
                    // 데이터 처리 위임
                    it->second->process_data();
                }
            }
        } catch (const dds::core::TimeoutError&) {
        } catch (const std::exception& e) {
            LOG_ERR("WaitSetDispatcher", "Data thread exception: %s", e.what());
        }
    }
}

} // namespace async
} // namespace rtpdds
