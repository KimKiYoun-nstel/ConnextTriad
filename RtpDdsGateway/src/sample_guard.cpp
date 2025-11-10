/**
 * @file sample_guard.cpp
 * @brief SampleGuard 구현 파일
 */

#include "sample_guard.hpp"
#include "sample_factory.hpp"
#include "triad_log.hpp"

namespace rtpdds
{
SampleGuard::SampleGuard(const std::string& type_name)
    : type_name_(type_name), sample_(nullptr)
{
    sample_ = create_sample(type_name_);
    if (sample_) {
        LOG_FLOW("created sample for type=%s ptr=%p", type_name_.c_str(), sample_);
    } else {
        LOG_ERR("SampleGuard", "failed to create sample for type=%s", type_name_.c_str());
    }
}

SampleGuard::~SampleGuard()
{
    if (sample_) {
        LOG_FLOW("destroying sample for type=%s ptr=%p", type_name_.c_str(), sample_);
        destroy_sample(type_name_, sample_);
        sample_ = nullptr;
    }
}

SampleGuard::SampleGuard(SampleGuard&& other) noexcept
    : type_name_(std::move(other.type_name_)), sample_(other.sample_)
{
    other.sample_ = nullptr;
}

SampleGuard& SampleGuard::operator=(SampleGuard&& other) noexcept
{
    if (this != &other) {
        // 기존 리소스 해제
        if (sample_) {
            destroy_sample(type_name_, sample_);
        }
        // 이동
        type_name_ = std::move(other.type_name_);
        sample_ = other.sample_;
        other.sample_ = nullptr;
    }
    return *this;
}

void* SampleGuard::release() noexcept
{
    void* ptr = sample_;
    sample_ = nullptr;
    LOG_FLOW("released ownership of sample type=%s ptr=%p", type_name_.c_str(), ptr);
    return ptr;
}

}  // namespace rtpdds
