#pragma once
/**
 * @file dds_receiver_interface.hpp
 * @brief DDS 수신기 추상화 인터페이스
 */
namespace rtpdds { namespace async {

/** 수신 방식 열거자 */
enum class DdsReceiveMode { Listener, WaitSet };

/**
 * @brief DDS 수신기 추상 인터페이스
 *
 * 구현체는 Listener 기반 또는 WaitSet 기반으로 제공될 수 있으며,
 * activate()/deactivate()를 통해 수신을 제어합니다.
 */
struct IDdsReceiver {
    /** 다형적 소멸자(안전한 delete 보장) */
    virtual ~IDdsReceiver() = default;

    /** 수신 시작: 리스너 등록 또는 WaitSet 루프 활성화 등을 수행 */
    virtual void activate() = 0;

    /** 수신 중지: 리스너 해제 또는 WaitSet 루프 종료 및 자원 해제 */
    virtual void deactivate() = 0;
};

}} // namespace
