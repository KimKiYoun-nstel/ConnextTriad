#pragma once
#include <functional>
#include <string>
#include <unordered_map>

namespace rtpdds {

struct TypeRegistry {
    struct Entry {
        std::function<const char *()> get_type_name;
        std::function<bool(void *participant)> register_type; // DDSDomainParticipant*
        std::function<void *(void *publisher, void *topic)> create_writer; // DDSDataWriter*
        std::function<void *(void *subscriber, void *topic)> create_reader; // DDSDataReader*
        std::function<bool(void *writer, const std::string &)> publish_from_text;
        std::function<std::string(void *reader)> take_one_to_display;
    };

    std::unordered_map<std::string, Entry> by_name;
};

} // namespace rtpdds


