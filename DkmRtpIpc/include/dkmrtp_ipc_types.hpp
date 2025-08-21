/**
 * @file dkmrtp_ipc_types.hpp
 * ### 파일 설명(한글)
 * IPC에서 사용하는 공용 타입 정의(Endpoint 등)
 * * 주소/포트 표현과 역할 구분(Enum)을 담는다.

 */
#pragma once
#include <cstdint>
#include <string>
namespace dkmrtp {
    namespace ipc {
        enum class Role { Server, Client };
        struct Endpoint {
            std::string address;
            uint16_t port{25000};
        };
    } // namespace ipc
} // namespace dkmrtp