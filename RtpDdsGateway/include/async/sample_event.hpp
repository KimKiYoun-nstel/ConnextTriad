#pragma once
#include <string>
#include <vector>
#include <chrono>
#include <cstdint>
#include <atomic>
#include "../dds_type_registry.hpp" // AnyData = std::any

namespace rtpdds { namespace async {

/**
 * @file sample_event.hpp
 * @brief 이벤트 구조체: SampleEvent / CommandEvent / ErrorEvent
 */

/**
 * @brief DDS 토픽에서 수신한 샘플을 표현합니다.
 *
 * `data`는 AnyData(예: std::any)로 토픽별 타입을 담습니다. `sequence_id`는
 * 이벤트 순서를 위해 내부적으로 증가하는 일련번호를 제공합니다.
 */
struct SampleEvent {
    std::string topic;
    std::string type_name;
    AnyData     data;
    std::chrono::steady_clock::time_point received_time;
    uint64_t sequence_id;

    static uint64_t next_sequence_id() {
        static std::atomic<uint64_t> c{0};
        return ++c;
    }

    SampleEvent() = default;
    SampleEvent(std::string t, std::string tn, AnyData d)
        : topic(std::move(t))
        , type_name(std::move(tn))
        , data(std::move(d))
        , received_time(std::chrono::steady_clock::now())
        , sequence_id(next_sequence_id()) {}
};

/**
 * @brief CommandEvent
 *
 * 외부 IPC/명령 요청을 표현합니다.
 * - `body`는 CBOR/JSON 원문이며 `is_cbor`로 구분합니다.
 * - `route`/`remote`는 요청의 경로/원격 식별자(예: "ipc", "tcp://...")를 담습니다.
 */
struct CommandEvent {
    uint32_t corr_id {0};
    std::string route;       // 예: "ipc"
    std::string remote;      // 예: "tcp://127.0.0.1:5555"
    std::vector<uint8_t> body;  // CBOR 또는 JSON 원문
    bool is_cbor {true};

    std::chrono::steady_clock::time_point received_time {
        std::chrono::steady_clock::now()
    };
};

/**
 * @brief ErrorEvent
 *
 * 내부 에러/상황을 외부에 알리기 위한 단순 구조체입니다.
 * - `where`는 에러 발생 위치 또는 컨텍스트, `what`에는 설명을 담습니다.
 */
struct ErrorEvent {
    std::string where;
    std::string what;
    std::chrono::steady_clock::time_point when {
        std::chrono::steady_clock::now()
    };
};

}} // namespace
