/**
 * @file async_event_processor.hpp
 * @brief 샘플/명령/에러 이벤트를 비동기 큐로 처리하는 워커 스레드 컴포넌트
 */
#pragma once
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>

#include "sample_handler.hpp"
#include "triad_log.hpp"

namespace rtpdds
{
namespace async
{

/**
 * @class AsyncEventProcessor
 * @brief 비동기 이벤트(샘플/명령/에러) 처리 큐와 워커를 제공하는 컴포넌트
 *
 * - post()로 작업을 큐잉하고, 내부 worker 스레드가 순차 처리합니다.
 * - monitor 스레드는 주기적으로 통계를 출력합니다(옵션).
 * - stop() 시 drain_stop 설정에 따라 큐 드레인 또는 즉시 종료합니다.
 */
class AsyncEventProcessor
{
   public:
    /**
     * @brief 구성 옵션
     * @param max_queue 최대 큐 깊이
     * @param monitor_sec 통계 로그 주기(초, 0=비활성)
     * @param drain_stop stop() 호출 시 큐를 드레인할지 여부
     * @param exec_warn_us 작업 처리 시간 경고 임계(마이크로초)
     */
    struct Config {
        size_t max_queue = 8192;
        int monitor_sec = 10;
        bool drain_stop = true;
        uint32_t exec_warn_us = 2000;
    };

    /**
     * @brief 비동기 이벤트 프로세서
     *
     * 비동기 작업 큐(프로듀서: post(), 컨슈머: 내부 worker 스레드)를 제공하여
     * 샘플/커맨드/에러 이벤트를 비동기식으로 처리합니다.
     *
     * 스레드 안전성: post()/set_*_handler()는 내부 락으로 보호됩니다.
     */
    explicit AsyncEventProcessor(const Config& cfg = {});
    ~AsyncEventProcessor();

    /** @brief 내부 worker와 모니터 스레드를 시작합니다. */
    void start();

    /** @brief 스레드를 종료합니다. drain_stop=false이면 큐를 드롭할 수 있습니다. */
    void stop();

    // 핸들러
    void set_handlers(const Handlers& hs)
    {
        std::lock_guard<std::mutex> lk(m_);
        sample_handler_ = hs.sample;
        cmd_handler_ = hs.command;
        error_handler_ = hs.error;
    }
    // 하위호환
    void set_sample_handler(SampleHandler h)
    {
        std::lock_guard<std::mutex> lk(m_);
        sample_handler_ = std::move(h);
    }
    void set_command_handler(CommandHandler h)
    {
        std::lock_guard<std::mutex> lk(m_);
        cmd_handler_ = std::move(h);
    }
    void set_error_handler(ErrorHandler h)
    {
        std::lock_guard<std::mutex> lk(m_);
        error_handler_ = std::move(h);
    }

    // 게시
    void post(const SampleEvent& ev)
    {
        stats_enq_sample_.fetch_add(1);
        enqueue([this, ev] {
            auto h = sample_handler_;
            if (h) h(ev);
        });
    }
    void post(const CommandEvent& ev)
    {
        stats_enq_cmd_.fetch_add(1);
        enqueue([this, ev] {
            auto h = cmd_handler_;
            if (h) h(ev);
        });
    }
    void post(const ErrorEvent& ev)
    {
        stats_enq_err_.fetch_add(1);
        enqueue([this, ev] {
            auto h = error_handler_;
            if (h) h(ev.what, ev.where);
        });
    }

    /**
     * @brief 처리 통계 스냅샷
     */
    struct Stats {
        uint64_t enq_sample, enq_cmd, enq_err, exec_jobs, dropped;
        size_t max_depth, cur_depth;
    };
    Stats get_stats() const
    {
        std::lock_guard<std::mutex> lk(m_);
        return {stats_enq_sample_.load(),
                stats_enq_cmd_.load(),
                stats_enq_err_.load(),
                stats_exec_.load(),
                stats_drop_.load(),
                max_depth_,
                q_.size()};
    }

    /** @brief worker가 실행 중인지 여부 */
    bool is_running() const;

   private:
    /** @brief 내부 큐에 작업을 추가(드롭 정책 포함) */
    void enqueue(std::function<void()> fn);
    /** @brief worker 스레드 루프 */
    void loop();
    /** @brief 모니터 스레드 루프(주기 통계 로그) */
    void monitor_loop();

    // 상태
    mutable std::mutex m_;
    std::condition_variable cv_;
    std::deque<std::function<void()> > q_;
    std::thread worker_, monitor_;
    std::atomic<bool> running_{false};

    // 핸들러
    SampleHandler sample_handler_;
    CommandHandler cmd_handler_;
    ErrorHandler error_handler_;

    // 통계/설정
    size_t max_depth_{0};
    std::atomic<uint64_t> stats_enq_sample_{0}, stats_enq_cmd_{0}, stats_enq_err_{0};
    std::atomic<uint64_t> stats_exec_{0}, stats_drop_{0};
    Config cfg_;
};

}  // namespace async
}  // namespace rtpdds
