#pragma once
#include <string>
#include "ndds/ndds_cpp.h"

namespace ngva {

// Arbitration/Registry 토픽용 핸들러 스켈레톤
struct ArbitrationHandler {
    explicit ArbitrationHandler(DDSDomainParticipant* dp);
    ~ArbitrationHandler();
    bool subscribe(const std::string& topic, const std::string& reader_profile);
    void poll_once(int timeout_ms); // WaitSet 1회 폴링 + 로깅

private:
    DDSDomainParticipant* dp_{nullptr};
    DDSTopic*             topic_{nullptr};
    DDSSubscriber*        sub_{nullptr};
    DDSDataReader*        rdr_{nullptr};
    DDSWaitSet            waitset_;
};

struct RegistryHandler {
    explicit RegistryHandler(DDSDomainParticipant* dp);
    ~RegistryHandler();
    bool subscribe(const std::string& topic, const std::string& reader_profile);
    void poll_once(int timeout_ms);

private:
    DDSDomainParticipant* dp_{nullptr};
    DDSTopic*             topic_{nullptr};
    DDSSubscriber*        sub_{nullptr};
    DDSDataReader*        rdr_{nullptr};
    DDSWaitSet            waitset_;
};

} // namespace ngva
