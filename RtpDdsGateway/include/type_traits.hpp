#pragma once

#ifdef USE_CONNEXT
#include "StringMsgSupport.h"
#include "AlarmMsgSupport.h"
#include <ndds/ndds_cpp.h>
#endif

namespace rtpdds {

// 전방 선언: 템플릿 기본형 (특수화로만 사용)
template <typename T> struct TypeTraits;

#ifdef USE_CONNEXT
// 기본 제공 StringMsg 특수화 (예시)
template <> struct TypeTraits<StringMsg> {
    static const char *name() { return StringMsgTypeSupport::get_type_name(); }
    static bool register_type(DDSDomainParticipant *p) {
        return StringMsgTypeSupport::register_type(p, name()) == DDS_RETCODE_OK;
    }
    using DataWriter = StringMsgDataWriter;
    using DataReader = StringMsgDataReader;
    using Seq        = StringMsgSeq;
};

template <> struct TypeTraits<AlarmMsg> {
    static const char *name() { return AlarmMsgTypeSupport::get_type_name(); }
    static bool register_type(DDSDomainParticipant *p) {
        return AlarmMsgTypeSupport::register_type(p, name()) == DDS_RETCODE_OK;
    }
    using DataWriter = AlarmMsgDataWriter;
    using DataReader = AlarmMsgDataReader;
    using Seq        = AlarmMsgSeq;
};
#endif

} // namespace rtpdds


