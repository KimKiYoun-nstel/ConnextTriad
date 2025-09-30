#pragma once
#include <memory>
#include "dds_receiver_interface.hpp"

/**
 * @file receiver_factory.hpp
 * @brief IDdsReceiver 팩토리
 */
namespace rtpdds { class DdsManager; }
namespace rtpdds { namespace async {

/**
 * @brief 요청된 모드에 맞는 IDdsReceiver를 생성합니다.
 * @param mode 수신 방식
 * @param mgr DdsManager(현재는 미사용 스텁)
 * @return unique_ptr<IDdsReceiver>
 */
std::unique_ptr<IDdsReceiver>
create_receiver(DdsReceiveMode mode, rtpdds::DdsManager& mgr);

}} // namespace
