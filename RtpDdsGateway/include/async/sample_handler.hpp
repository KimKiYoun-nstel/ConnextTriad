#pragma once
#include <functional>
#include "sample_event.hpp"

namespace rtpdds { namespace async {

/**
 * @file sample_handler.hpp
 * @brief 이벤트 처리기 타입 정의
 */

/**
 * @brief SampleHandler: DDS 샘플 처리 함수(참조 전달)
 */
using SampleHandler = std::function<void(const SampleEvent&)>;

using ErrorHandler  = std::function<void(const std::string& what,
                                         const std::string& where)>;

using CommandHandler = std::function<void(const CommandEvent&)>;
// TODO(next): DdsOutputEvent/IpcOutputEvent 필요 시 정의

}} // namespace

namespace rtpdds { namespace async {

/**
 * @brief Handlers 구조체
 *
 * AsyncEventProcessor에 단일 구조로 주입됩니다.
 * 주입 방식은 간단하며, 런타임에 핸들러를 교체할 수 있습니다.
 */
struct Handlers {
    SampleHandler  sample;
    CommandHandler command;
    ErrorHandler   error;
};

}} // namespace
