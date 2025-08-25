#include "ngva/ngva_handlers.hpp"
#include "ngva/ngva_dynamic_probe.hpp"
#include <cstdio>

using namespace ngva;

static DDSTopic* ensure_topic(DDSDomainParticipant* dp, const std::string& name)
{
    // NGVA의 RMDP에서 타입 이름/토픽 이름이 고정됨.
    // 정적 타입(코드 생성)이 있다면 여기서 type_name을 정확히 지정하세요.
    // 동적 경로만 먼저 쓰는 경우, TypeName을 비워두고 DynamicData 전파에 의존할 수 있습니다.
    const char* type_name = ""; // (DynamicData fallback)
    DDSTopic* t = dp->find_topic(name.c_str(), DDS_DURATION_ZERO);
    if (t) return t;

    // NOTE: 정적 타입을 쓸 때는 register_type 호출 후 create_topic 하세요.
    t = dp->create_topic(
        name.c_str(), type_name, DDS_TOPIC_QOS_DEFAULT, NULL, DDS_STATUS_MASK_NONE);
    return t;
}

static DDSSubscriber* ensure_sub(DDSDomainParticipant* dp)
{
    DDSSubscriber* s = dp->create_subscriber(
        DDS_SUBSCRIBER_QOS_DEFAULT, NULL, DDS_STATUS_MASK_NONE);
    return s;
}

ArbitrationHandler::ArbitrationHandler(DDSDomainParticipant* dp) : dp_(dp) {}
ArbitrationHandler::~ArbitrationHandler() {
    if (rdr_) sub_->delete_datareader(rdr_);
    if (topic_) dp_->delete_topic(topic_);
    if (sub_) dp_->delete_subscriber(sub_);
}
bool ArbitrationHandler::subscribe(const std::string& topic, const std::string& /*reader_profile*/)
{
    topic_ = ensure_topic(dp_, topic);
    if (!topic_) return false;
    sub_ = ensure_sub(dp_);
    if (!sub_) return false;

    // 동적 구독: Generic DynamicData Reader 생성
    rdr_ = create_dynamic_reader(sub_, topic_);
    if (!rdr_) return false;

    // 상태 조건 부착
    DDSStatusCondition* sc = rdr_->get_statuscondition();
    sc->set_enabled_statuses(DDS_DATA_AVAILABLE_STATUS);
    waitset_.attach_condition(sc);
    return true;
}
void ArbitrationHandler::poll_once(int timeout_ms)
{
    poll_dynamic_reader_once(rdr_, timeout_ms);
}

RegistryHandler::RegistryHandler(DDSDomainParticipant* dp) : dp_(dp) {}
RegistryHandler::~RegistryHandler() {
    if (rdr_) sub_->delete_datareader(rdr_);
    if (topic_) dp_->delete_topic(topic_);
    if (sub_) dp_->delete_subscriber(sub_);
}
bool RegistryHandler::subscribe(const std::string& topic, const std::string& /*reader_profile*/)
{
    topic_ = ensure_topic(dp_, topic);
    if (!topic_) return false;
    sub_ = ensure_sub(dp_);
    if (!sub_) return false;

    rdr_ = create_dynamic_reader(sub_, topic_);
    if (!rdr_) return false;

    DDSStatusCondition* sc = rdr_->get_statuscondition();
    sc->set_enabled_statuses(DDS_DATA_AVAILABLE_STATUS);
    waitset_.attach_condition(sc);
    return true;
}
void RegistryHandler::poll_once(int timeout_ms)
{
    poll_dynamic_reader_once(rdr_, timeout_ms);
}
