#pragma once
#include <string>

namespace rtpdds {

template <typename T> struct DataConverter;

#ifdef USE_CONNEXT
#include "StringMsgSupport.h"
#include "AlarmMsgSupport.h"

template <> struct DataConverter<StringMsg> {
    static StringMsg from_text(const std::string &text) {
        StringMsg m{};
        m.text = (char *)DDS_String_dup(text.c_str());
        return m;
    }
    static std::string to_display(const StringMsg &m) {
        return std::string(m.text ? m.text : "");
    }
    static void cleanup(StringMsg &m) {
        if (m.text) { DDS_String_free(m.text); m.text = nullptr; }
    }
};

template <> struct DataConverter<AlarmMsg> {
    static AlarmMsg from_text(const std::string &text) {
        AlarmMsg m{};
        m.level = 1;
        m.text = (char *)DDS_String_dup(text.c_str());
        return m;
    }
    static std::string to_display(const AlarmMsg &m) {
        return std::string("Level: ") + std::to_string(m.level) + ", Text: " + (m.text ? m.text : "");
    }
    static void cleanup(AlarmMsg &m) {
        if (m.text) { DDS_String_free(m.text); m.text = nullptr; }
    }
};
#endif

} // namespace rtpdds


