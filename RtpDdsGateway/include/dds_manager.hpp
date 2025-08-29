/**
 * @file dds_manager.hpp
 * ### 파일 설명(한글)
 * DDS 엔티티(Participant/Publisher/Subscriber/Topic/Writer/Reader) 생성 및 관리.
 * * UI(IPC)로부터 명령을 받아 DDS 동작을 수행하고, 필요시 수신 샘플을 콜백으로 올려준다.


 */
#pragma once
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

#include <ndds/ndds_cpp.h>
#include "AlarmMsgSupport.h"
#include "StringMsgSupport.h"
#include "data_converter.hpp"
#include "register_macro.hpp"
#include "type_registry.hpp"
#include "type_traits.hpp"


namespace rtpdds {
enum class DdsErrorCategory { None, Resource, Logic };
struct DdsResult {
    bool ok;
    DdsErrorCategory category;
    std::string reason;
    DdsResult(bool success = true, DdsErrorCategory cat = DdsErrorCategory::None, std::string msg = "")
        : ok(success), category(cat), reason(std::move(msg)) {}
};
/** @brief DDS 구성의 수명과 소유권을 관리하는 핵심 매니저 클래스 */
class DdsManager {
   public:
    DdsManager();
    ~DdsManager();
    // N개 관리 구조: 도메인ID별 participant, 이름별 pub/sub, topic별 writer/reader
    DdsResult create_participant(int domain_id, const std::string &qos_lib, const std::string &qos_profile);
    DdsResult create_publisher(int domain_id, const std::string &pub_name, const std::string &qos_lib,
                          const std::string &qos_profile);
    DdsResult create_subscriber(int domain_id, const std::string &sub_name, const std::string &qos_lib,
                           const std::string &qos_profile);
    DdsResult create_writer(int domain_id, const std::string &pub_name, const std::string &topic,
                       const std::string &type_name, const std::string &qos_lib, const std::string &qos_profile);
    DdsResult create_reader(int domain_id, const std::string &sub_name, const std::string &topic,
                       const std::string &type_name, const std::string &qos_lib, const std::string &qos_profile);
    DdsResult publish_text(const std::string &topic, const std::string &text);
    DdsResult publish_text(int domain_id, const std::string &pub_name, const std::string &topic, const std::string &text);

   public:
    using SampleHandler =
        std::function<void(const std::string &topic, const std::string &type_name, const std::string &display)>;
    void set_on_sample(SampleHandler cb);

   private:
    // Reader용 Listener
    struct ReaderListener : public DDSDataReaderListener {
        DdsManager &owner;
        std::string topic;
        explicit ReaderListener(DdsManager &o, std::string t) : owner(o), topic(std::move(t)) {}
        void on_data_available(DDSDataReader *reader) override;
    };

    std::unordered_map<int, DDSDomainParticipant *>                           participants_;
    std::unordered_map<int, std::unordered_map<std::string, DDSPublisher *>>  publishers_;
    std::unordered_map<int, std::unordered_map<std::string, DDSSubscriber *>> subscribers_;
    std::unordered_map<int, std::unordered_map<std::string, std::unordered_map<std::string, DDSDataWriter *>>> writers_;
    std::unordered_map<int, std::unordered_map<std::string, std::unordered_map<std::string, DDSDataReader *>>> readers_;

    std::unordered_map<std::string, ReaderListener *> listeners_;
    SampleHandler                                     on_sample_;
    // 타입 레지스트리 및 topic->type 매핑
    TypeRegistry                                 registry_{};
    std::unordered_map<std::string, std::string> topic_to_type_{};
};
}  // namespace rtpdds
