/**
 * @file dkmrtp_ipc_messages.hpp
 * ### 파일 설명(한글)
 * IPC 프로토콜 메시지 정의(헤더/명령/응답/이벤트).
 * * 모든 구조체는 패킹 없이 사용되며 네트워크 바이트 오더와 호환되는 필드만 사용.

 */
#pragma once
#include <cstdint>
namespace dkmrtp {
    namespace ipc {
#pragma pack(push, 1)
        /// 메시지 공통 헤더(네트워크 전송 단위의 프레임 앞부분)
struct Header {
                uint32_t magic{0x52495043}; ///< 'RIPC' 매직 값(프레이밍 검증)
                uint16_t version{0x0001};      ///< 프로토콜 버전
                uint16_t type{0};         ///< 메시지 타입(명령/응답/이벤트)
                uint32_t corr_id{0};     ///< 요청/응답 상관관계 식별자
                uint32_t length{0};      ///< 페이로드 길이(바이트)
                uint64_t ts_ns{0};       ///< 송신측 타임스탬프(ns) (디버깅용)
        };
#pragma pack(pop)
        enum : uint16_t {
            MSG_CMD_HELLO = 0x0301,
            MSG_CMD_PARTICIPANT_CREATE = 0x0101,
            MSG_CMD_PUBLISHER_CREATE = 0x0102,
            MSG_CMD_SUBSCRIBER_CREATE = 0x0103,
            MSG_CMD_PUBLISH_SAMPLE = 0x0104,
            MSG_CMD_SHUTDOWN = 0x01FF,
            MSG_EVT_DATA = 0x0201,
            MSG_RSP_ACK = 0x0202,
            MSG_RSP_ERROR = 0x0203,
            MSG_CTRL_HEALTH = 0x0302,
            MSG_CTRL_FLOW = 0x0303,

            // ===== Unified RPC envelope frame types (for CBOR/JSON payload)
            // =====
            MSG_FRAME_REQ = 0x1000, // Request frame (payload: CBOR/JSON)
            MSG_FRAME_RSP = 0x1001, // Response frame (payload: CBOR/JSON)
            MSG_FRAME_EVT = 0x1002 // Event frame (payload: CBOR/JSON)
        };
#pragma pack(push, 1)
        struct CmdParticipantCreate {
            int32_t domain_id{0};
            char qos_library[128] = {0};
            char qos_profile[128] = {0};
        };
        struct CmdPublisherCreate {
            char topic[64] = {0};
            char type_name[64] = {0};
            char qos_library[128] = {0};
            char qos_profile[128] = {0};
        };
        struct CmdSubscriberCreate {
            char topic[64] = {0};
            char type_name[64] = {0};
            char qos_library[128] = {0};
            char qos_profile[128] = {0};
        };
        struct CmdPublishSample {
            char topic[64] = {0};
            char type_name[64] = {0};
            uint32_t content_len{0};
        };
        struct RspError {
            uint32_t err_code{0};
        };
#pragma pack(pop)
    } // namespace ipc
} // namespace dkmrtp