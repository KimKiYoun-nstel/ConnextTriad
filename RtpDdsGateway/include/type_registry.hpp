#pragma once
/**
 * @file type_registry.hpp
 * @brief 토픽 타입별 등록/생성/헬퍼의 중앙 레지스트리, 템플릿 기반 확장 구조
 *
 * 토픽 타입별로 이름, 타입 등록, writer/reader 생성, publish/take 헬퍼 등을 중앙에서 관리합니다.
 * 신규 토픽/타입을 추가할 때, 이 레지스트리에 등록하면 전체 시스템에서 일관되게 활용할 수 있습니다.
 *
 * 연관 파일:
 *   - dds_type_registry.hpp (엔티티 동적 생성)
 *   - dds_manager.hpp (엔티티 관리)
 */
#include <functional>
#include <string>
#include <unordered_map>

namespace rtpdds {

/**
 * @struct TypeRegistry
 * @brief 토픽 타입별 등록/생성/헬퍼의 중앙 레지스트리
 *
 * Entry 구조체에 타입별 생성/등록/헬퍼 함수 포인터를 저장하며,
 * by_name 맵을 통해 토픽명으로 접근할 수 있습니다.
 */
struct TypeRegistry {
    /**
     * @struct Entry
     * @brief 타입별 생성/등록/헬퍼 함수 포인터 집합
     *
     * - get_type_name: 타입명 반환
     * - register_type: 타입 등록
     * - create_writer: writer 생성
     * - create_reader: reader 생성
     * - publish_from_text: 텍스트 기반 publish
     * - take_one_to_display: 샘플 1건 표시용 추출
     */
    struct Entry {
        std::function<const char *()> get_type_name;
        std::function<bool(void *participant)> register_type; // DDSDomainParticipant*
        std::function<void *(void *publisher, void *topic)> create_writer; // DDSDataWriter*
        std::function<void *(void *subscriber, void *topic)> create_reader; // DDSDataReader*
        std::function<bool(void *writer, const std::string &)> publish_from_text;
        std::function<std::string(void *reader)> take_one_to_display;
    };

    /**
     * @brief 토픽명 → Entry 매핑 테이블
     */
    std::unordered_map<std::string, Entry> by_name;
};

} // namespace rtpdds


