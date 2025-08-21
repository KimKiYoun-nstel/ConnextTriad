#pragma once
// ConnextControlUI/include/rpc_envelope.hpp
// UI에서 스키마 의존을 최소화하기 위한 RPC 빌더(헤더온리)

#include "nlohmann/json.hpp" // CMake에 ${CMAKE_SOURCE_DIR}/third_party 포함되어 있어야 함
#include <map>
#include <string>
#include <vector>

namespace triad {
    namespace rpc {

        /**
         * @brief RPC Envelope 빌더
         * - UI에서는 JSON 키를 직접 다루지 않고, 체이닝 API로 의미만 지정
         * - 내부적으로 JSON을 구성하고, 전송용 CBOR 바이트를 생성
         */
        class RpcBuilder {
          public:
            RpcBuilder &set_op(const std::string &op) {
                j_["op"] = op;
                return *this;
            }
            RpcBuilder &
            set_target(const std::string &kind,
                       const std::map<std::string, std::string> &kv = {}) {
                nlohmann::json tgt;
                tgt["kind"] = kind;
                for (auto &[k, v] : kv)
                    tgt[k] = v;
                j_["target"] = std::move(tgt);
                return *this;
            }
            RpcBuilder &args(const std::map<std::string, nlohmann::json> &kv) {
                for (auto &[k, v] : kv)
                    j_["args"][k] = v;
                return *this;
            }
            RpcBuilder &data(const std::map<std::string, nlohmann::json> &kv) {
                for (auto &[k, v] : kv)
                    j_["data"][k] = v;
                return *this;
            }
            RpcBuilder &proto(int v = 1) {
                j_["proto"] = v;
                return *this;
            }

            // 전송용 CBOR 바이트
            std::vector<uint8_t> to_cbor() const {
                return nlohmann::json::to_cbor(j_);
            }
            // 로그용 JSON 문자열 (pretty=false면 한 줄)
            std::string to_json(bool pretty = false, int indent = 2) const {
                return j_.dump(pretty ? indent : 0);
            }
            const nlohmann::json &json() const {
                return j_;
            }

          private:
            nlohmann::json j_;
        };

    } // namespace rpc
} // namespace triad
