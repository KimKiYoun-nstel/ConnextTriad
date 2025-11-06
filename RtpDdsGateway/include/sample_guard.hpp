#pragma once
/**
 * @file sample_guard.hpp
 * @brief DDS 샘플 메모리 자동 관리를 위한 RAII 래퍼
 *
 * RAII(Resource Acquisition Is Initialization) 패턴을 활용하여
 * DDS 샘플 메모리의 생성/해제를 자동화하고 예외 안전성을 보장합니다.
 *
 * 사용 예:
 * @code
 *   SampleGuard guard(type_name);
 *   if (!guard.get()) return error;
 *   json_to_dds(j, type_name, guard.get());
 *   writer->write(...);
 *   // 스코프 벗어나면 자동 해제 (예외 발생 시에도 안전)
 * @endcode
 */

#include <string>

namespace rtpdds
{
/**
 * @class SampleGuard
 * @brief DDS 샘플 메모리의 생명주기를 자동 관리하는 RAII 클래스
 *
 * 생성 시 create_sample() 호출, 소멸 시 destroy_sample() 자동 호출.
 * 복사 금지, 이동 허용.
 */
class SampleGuard
{
public:
    /**
     * @brief 샘플 생성 및 초기화
     * @param type_name DDS 타입명
     */
    explicit SampleGuard(const std::string& type_name);

    /**
     * @brief 소멸자: 샘플 자동 해제
     */
    ~SampleGuard();

    // 복사 금지 (소유권 중복 방지)
    SampleGuard(const SampleGuard&) = delete;
    SampleGuard& operator=(const SampleGuard&) = delete;

    // 이동 허용 (소유권 이전)
    SampleGuard(SampleGuard&& other) noexcept;
    SampleGuard& operator=(SampleGuard&& other) noexcept;

    /**
     * @brief 샘플 포인터 반환
     * @return void* 샘플 포인터 (생성 실패 시 nullptr)
     */
    void* get() const noexcept { return sample_; }

    /**
     * @brief 샘플 포인터 반환 (명시적)
     * @return void* 샘플 포인터 (생성 실패 시 nullptr)
     */
    void* data() const noexcept { return sample_; }

    /**
     * @brief 유효성 확인
     * @return bool 샘플이 성공적으로 생성되었는지 여부
     */
    explicit operator bool() const noexcept { return sample_ != nullptr; }

    /**
     * @brief 소유권 해제 (수동 관리로 전환)
     * @return void* 샘플 포인터 (호출자가 destroy_sample() 호출 책임)
     */
    void* release() noexcept;

private:
    std::string type_name_;
    void* sample_;
};

}  // namespace rtpdds
