#include "async/receiver_factory.hpp"
#include "triad_log.hpp"

namespace rtpdds { class DdsManager; }

namespace rtpdds { namespace async {

namespace {

struct ListenerReceiver final : IDdsReceiver {
    void activate()   override { LOG_INF("DDSRX","ListenerReceiver activate"); }
    void deactivate() override { LOG_INF("DDSRX","ListenerReceiver deactivate"); }
};

struct WaitSetReceiver final : IDdsReceiver {
    void activate()   override { LOG_WRN("DDSRX","WaitSetReceiver activate (stub)"); }
    void deactivate() override { LOG_WRN("DDSRX","WaitSetReceiver deactivate (stub)"); }
};

} // anon

std::unique_ptr<IDdsReceiver>
create_receiver(DdsReceiveMode mode, rtpdds::DdsManager& /*mgr*/)
{
    switch (mode) {
        case DdsReceiveMode::Listener: return std::make_unique<ListenerReceiver>();
        case DdsReceiveMode::WaitSet:  return std::make_unique<WaitSetReceiver>();
        default:                       return std::make_unique<ListenerReceiver>();
    }
}

}} // namespace
