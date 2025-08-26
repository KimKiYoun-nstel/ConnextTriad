/**
 * @file dds_manager.hpp
 * ### 파일 설명(한글)
 * DDS 엔티티(Participant/Publisher/Subscriber/Topic/Writer/Reader) 생성 및 관리.
 * * UI(IPC)로부터 명령을 받아 DDS 동작을 수행하고, 필요시 수신 샘플을 콜백으로 올려준다.
 * * RTI Connext Classic C++ API 사용. `USE_CONNEXT`가 꺼진 경우 스텁 동작.

 */
#pragma once
#include <memory>
#include <string>
#include <unordered_map>
#include <functional> // ⬅️ 이 줄을 추가하여 std::function을 사용할 수 있게 합니다.
#include "type_registry.hpp"
#include "type_traits.hpp"
#include "data_converter.hpp"
#include "register_macro.hpp"
#ifdef USE_CONNEXT
#include "StringMsgSupport.h"
#include "AlarmMsgSupport.h"
#include <ndds/ndds_cpp.h>
#endif
namespace rtpdds {
    /** @brief DDS 구성의 수명과 소유권을 관리하는 핵심 매니저 클래스 */
class DdsManager {
      public:
        DdsManager();
        ~DdsManager();
        bool create_participant(int domain_id, const std::string &qos_lib,
                                const std::string &qos_profile);
        bool create_writer(const std::string &topic,
                           const std::string &type_name,
                           const std::string &qos_lib,
                           const std::string &qos_profile);
        bool create_reader(const std::string &topic,
                           const std::string &type_name,
                           const std::string &qos_lib,
                           const std::string &qos_profile);
        bool publish_text(const std::string &topic, const std::string &text);

      public:
        using SampleHandler = std::function<void(const std::string &topic,
                                                 const std::string &type_name,
                                                 const std::string &display)>;
        void set_on_sample(SampleHandler cb);


      private:
#ifdef USE_CONNEXT
        // Reader용 Listener
        struct ReaderListener : public DDSDataReaderListener {
            DdsManager &owner;
            std::string topic;
            explicit ReaderListener(DdsManager &o, std::string t)
                : owner(o), topic(std::move(t)) {
            }
            void on_data_available(DDSDataReader *reader) override;
        };

        DDSDomainParticipant *participant_{nullptr};
        DDSPublisher *publisher_{nullptr};
        DDSSubscriber *subscriber_{nullptr};
        std::unordered_map<std::string, DDSDataWriter *> writers_;
        std::unordered_map<std::string, DDSDataReader *> readers_;
        std::unordered_map<std::string, ReaderListener *> listeners_; // ⬅️ 추가
        SampleHandler on_sample_;                                     // ⬅️ 추가
        // 타입 레지스트리 및 topic->type 매핑
        TypeRegistry registry_{};
        std::unordered_map<std::string, std::string> topic_to_type_{};
#endif
    };
} // namespace rtpdds