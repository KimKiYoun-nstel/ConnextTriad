/**
 * @file triad_thread.hpp
 * @brief VxWorks 환경에서 사용자 스택 크기(1024KB)를 강제 적용하기 위한 thread 래퍼.
 *
 * VxWorks 기본 task/스레드 스택 크기(~20KB 수준)로 인해 실행 중 스택 한계가
 * 근접하는 문제가 발견됨. 본 래퍼는 VxWorks 환경에서 pthread_attr_setstacksize()
 * 를 통해 1024KB(1MB) 스택을 명시적으로 부여한다. 다른 OS(Windows/Linux 등)에서는
 * 표준 std::thread를 그대로 사용한다.
 *
 * 사용 규칙:
 *  - 프로젝트 내 직접 생성하는 모든 스레드(메인 이후 시작)는 TriadThread 사용
 *  - RTI Connext DDS 내부에서 생성되는 스레드는 변경 제외
 *  - joinable()/join() 인터페이스는 std::thread 와 유사하게 동작
 */
#pragma once

#include <functional>
#include <memory>

#ifdef RTI_VXWORKS
#include <pthread.h>

namespace triad {

/** @brief VxWorks 전용 스택 크기(1024KB) */
constexpr size_t TRIAD_THREAD_STACK_SIZE = 1024 * 1024; // 1MB

/**
 * @class TriadThread
 * @brief VxWorks에서 지정된 스택 크기로 pthread를 생성하는 래퍼.
 */
class TriadThread {
public:
    TriadThread() = default;
    TriadThread(const TriadThread&) = delete;
    TriadThread& operator=(const TriadThread&) = delete;
    TriadThread(TriadThread&& other) noexcept { move_from(std::move(other)); }
    TriadThread& operator=(TriadThread&& other) noexcept {
        if (this != &other) { cleanup(); move_from(std::move(other)); }
        return *this;
    }
    ~TriadThread() { /* join은 명시적 호출 요구. 파괴 시 자동 join 하지 않음 */ }

    template <typename Fn>
    explicit TriadThread(Fn&& fn) { start(std::forward<Fn>(fn)); }

    template <typename Fn>
    void start(Fn&& fn) {
        cleanup();
        using FType = typename std::decay<Fn>::type;
        holder_.reset(new FnHolderImpl<FType>(std::forward<Fn>(fn)));
        pthread_attr_t attr; pthread_attr_init(&attr);
        // 스택 크기 지정 (실패 시 기본값 사용되나 로그/에러 처리는 상위 레벨에서 가능)
        pthread_attr_setstacksize(&attr, TRIAD_THREAD_STACK_SIZE);
        int rc = pthread_create(&tid_, &attr, &TriadThread::trampoline, holder_.get());
        pthread_attr_destroy(&attr);
        started_ = (rc == 0);
    }

    bool joinable() const { return started_; }
    void join() {
        if (started_) { pthread_join(tid_, nullptr); started_ = false; }
    }

private:
    struct FnHolderBase { virtual ~FnHolderBase() = default; virtual void run() = 0; };
    template <typename F> struct FnHolderImpl : FnHolderBase {
        F f; explicit FnHolderImpl(F&& fn): f(std::move(fn)) {}
        void run() override { f(); }
    };

    static void* trampoline(void* arg) {
        auto* base = static_cast<FnHolderBase*>(arg);
        if (base) base->run();
        return nullptr;
    }

    void cleanup() { /* pthread_join은 명시적 join 사용 */ holder_.reset(); started_ = false; }
    void move_from(TriadThread&& other) {
        tid_ = other.tid_; started_ = other.started_; holder_ = std::move(other.holder_); other.started_ = false; }

    pthread_t tid_{}; bool started_{false}; std::unique_ptr<FnHolderBase> holder_;
};

} // namespace triad

#else // !__VXWORKS__

#include <thread>
namespace triad {
/** @brief 비 VxWorks 환경에서는 std::thread 그대로 사용 */
using TriadThread = std::thread;
constexpr size_t TRIAD_THREAD_STACK_SIZE = 0; // 의미 없음
} // namespace triad

#endif
