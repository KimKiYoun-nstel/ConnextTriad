/**
 * @file gateway.cpp
 * @brief GatewayApp 구현 파일
 *
 * 서버/클라이언트 모드 진입과 종료 루틴을 담당.
 * 각 함수별로 파라미터/리턴/비즈니스 처리에 대한 상세 주석을 추가.
 */
#include "gateway.hpp"
#include <iostream>
#include <thread>
// async
#include "async/async_event_processor.hpp"
#include "async/sample_event.hpp"
// logging
#include "triad_log.hpp"
// adapter
#include "rtpdds/dds_manager_adapter.hpp"

#include "app_config.hpp"

namespace rtpdds {

/**
 * @brief GatewayApp 생성자
 * 내부적으로 DdsManager, IpcAdapter를 초기화할 준비만 함
 */
GatewayApp::GatewayApp()
  : mgr_(AppConfig::instance().dds().qos_dir),
    async_(async::AsyncEventProcessor::Config{
        /*max_queue*/   8192,
        /*monitor_sec*/ 10,
        /*drain_stop*/  true,
        /*exec_warn_us*/ 1000000 })  // 1000ms (1초)
{
    // 소비자 스레드 시작
    async_.start();

    // 단일 핸들러 주입
    async::Handlers hs;
    hs.sample = [this](const async::SampleEvent& ev) {
        // Phase 1-2: 큐 대기 시간 측정
        auto now = std::chrono::steady_clock::now();
        auto queue_delay_us = std::chrono::duration_cast<std::chrono::microseconds>(
            now - ev.received_time).count();
        
        if (queue_delay_us > 500000) {  // 500ms 이상 대기 시 경고
            LOG_WRN("ASYNC", "high_queue_delay sample topic=%s delay_us=%lld",
                    ev.topic.c_str(), (long long)queue_delay_us);
        }
        
        LOG_DBG("ASYNC", "sample exec topic=%s type=%s seq=%llu queue_delay_us=%lld",
                ev.topic.c_str(), ev.type_name.c_str(), 
                static_cast<unsigned long long>(ev.sequence_id),
                (long long)queue_delay_us);
        if (ipc_) ipc_->emit_evt_from_sample(ev);
    };
    hs.command = [this](const async::CommandEvent& ev) {
        // Phase 1-2: 큐 대기 시간 측정
        auto now = std::chrono::steady_clock::now();
        auto queue_delay_us = std::chrono::duration_cast<std::chrono::microseconds>(
            now - ev.received_time).count();
        
        if (queue_delay_us > 500000) {  // 500ms 이상 대기 시 경고
            LOG_WRN("ASYNC", "high_queue_delay cmd corr_id=%u delay_us=%lld",
                    ev.corr_id, (long long)queue_delay_us);
        }
        
        LOG_DBG("ASYNC", "cmd exec corr_id=%u size=%zu route=%s queue_delay_us=%lld",
                ev.corr_id, ev.body.size(), ev.route.c_str(),
                (long long)queue_delay_us);
        if (ipc_) ipc_->process_request(ev);
    };
    hs.error = [](const std::string& what, const std::string& where) {
        LOG_WRN("ASYNC", "error where=%s what=%s", where.c_str(), what.c_str());
    };
    async_.set_handlers(hs);

    // DDS -> 큐 적재 (엔큐 시점 로깅)
    mgr_.set_on_sample([this](const std::string& topic,
                              const std::string& type_name,
                              const AnyData& data) {
        async::SampleEvent ev{topic, type_name, data};
    LOG_DBG("ASYNC", "sample enq topic=%s type=%s seq=%llu",
        ev.topic.c_str(), ev.type_name.c_str(), static_cast<unsigned long long>(ev.sequence_id));
        async_.post(ev);
    });
}

/**
 * @brief 서버 모드 시작
 * @param bind 바인드 주소
 * @param port 바인드 포트
 * @return 서버 시작 성공 여부
 *
 * 내부적으로 IpcAdapter를 생성하고 서버 모드로 시작
 */
bool GatewayApp::start_server(const std::string &bind, uint16_t port) {
    if (!mgr_iface_) mgr_iface_ = std::make_unique<DdsManagerAdapter>(mgr_);
    if (!ipc_) ipc_ = std::make_unique<IpcAdapter>(*mgr_iface_);
    if (!rx_)  rx_  = async::create_receiver(rx_mode_, mgr_);
    rx_->activate();
    // IpcAdapter에 post 함수 연결 (엔큐 시점 로깅)
    ipc_->set_command_post([this](const async::CommandEvent& ev){
        LOG_DBG("ASYNC", "cmd enq corr_id=%u size=%zu", ev.corr_id, ev.body.size());
        async_.post(ev);
    });
    // 방어적 재시작 허용
    if (!async_.is_running()) async_.start();
    return ipc_->start_server(bind, port);
}

/**
 * @brief 수신 모드 설정
 * @note 이 함수는 Reader/Writer 등 DDS 엔티티 생성 전에 호출해야 합니다.
 */
void GatewayApp::set_receive_mode(async::DdsReceiveMode mode)
{
    rx_mode_ = mode;
    // DdsManager에도 동일한 모드를 설정
    using EventMode = rtpdds::DdsManager::EventMode;
    if (mode == async::DdsReceiveMode::Listener) {
        mgr_.set_event_mode(EventMode::Listener);
    } else {
        mgr_.set_event_mode(EventMode::WaitSet);
    }
}

/**
 * @brief 클라이언트 모드 시작
 * @param peer 접속할 서버 주소
 * @param port 접속할 서버 포트
 * @return 클라이언트 시작 성공 여부
 *
 * 내부적으로 IpcAdapter를 생성하고 클라이언트 모드로 시작
 */
bool GatewayApp::start_client(const std::string &peer, uint16_t port) {
    if (!mgr_iface_) mgr_iface_ = std::make_unique<DdsManagerAdapter>(mgr_);
    if (!ipc_) ipc_ = std::make_unique<IpcAdapter>(*mgr_iface_);
    if (!rx_)  rx_  = async::create_receiver(rx_mode_, mgr_);
    rx_->activate();
    ipc_->set_command_post([this](const async::CommandEvent& ev){
        LOG_FLOW("cmd enq corr_id=%u size=%zu", ev.corr_id, ev.body.size());
        async_.post(ev);
    });
    if (!async_.is_running()) async_.start();
    return ipc_->start_client(peer, port);
}

/**
 * @brief 메인 루프 실행
 *
 * 실제 서비스에서는 이벤트 루프/종료 신호 처리 등이 추가될 수 있음
 */
void GatewayApp::run() {
    std::cout << "[Gateway] running. Press Ctrl+C to exit.\n";
    while (true)
        std::this_thread::sleep_for(std::chrono::seconds(1)); // 단순 대기 루프
}

/**
 * @brief 종료 및 자원 해제
 *
 * IpcAdapter를 해제하여 IPC 서버/클라이언트 종료
 */
void GatewayApp::stop() {
    if (rx_) rx_->deactivate();
    // 종료 전 통계 스냅샷
    auto st = async_.get_stats();
    LOG_INF("ASYNC", "stats enq(sample/cmd/err)=(%llu/%llu/%llu) exec=%llu drop=%llu max_depth=%zu cur_depth=%zu",
            (unsigned long long)st.enq_sample,
            (unsigned long long)st.enq_cmd,
            (unsigned long long)st.enq_err,
            (unsigned long long)st.exec_jobs,
            (unsigned long long)st.dropped,
            st.max_depth, st.cur_depth);

    // 먼저 소비자 스레드 종료
    async_.stop();
    ipc_.reset();
}

} // namespace rtpdds
