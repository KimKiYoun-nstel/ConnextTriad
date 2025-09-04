#pragma once
#if 0
#include <ndds/ndds_cpp.h>

#include "AlarmMsgSupport.h"
#include "Alarms_PSMSupport.h"
#include "StringMsgSupport.h"

namespace rtpdds
{

// 전방 선언: 템플릿 기본형 (특수화로만 사용)
template <typename T>
struct TypeTraits;

// 기본 제공 StringMsg 특수화 (예시)
template <>
struct TypeTraits<StringMsg> {
    static const char* name()
    {
        return StringMsgTypeSupport::get_type_name();
    }
    static bool register_type(DDSDomainParticipant* p)
    {
        return StringMsgTypeSupport::register_type(p, name()) == DDS_RETCODE_OK;
    }
    using DataWriter = StringMsgDataWriter;
    using DataReader = StringMsgDataReader;
    using Seq = StringMsgSeq;
};

template <>
struct TypeTraits<AlarmMsg> {
    static const char* name()
    {
        return AlarmMsgTypeSupport::get_type_name();
    }
    static bool register_type(DDSDomainParticipant* p)
    {
        return AlarmMsgTypeSupport::register_type(p, name()) == DDS_RETCODE_OK;
    }
    using DataWriter = AlarmMsgDataWriter;
    using DataReader = AlarmMsgDataReader;
    using Seq = AlarmMsgSeq;
};

template <>
struct TypeTraits<P_Alarms_PSM_C_Actual_Alarm> {
    static const char* name()
    {
        return P_Alarms_PSM_C_Actual_AlarmTypeSupport::get_type_name();
    }
    static bool register_type(DDSDomainParticipant* p)
    {
        return P_Alarms_PSM_C_Actual_AlarmTypeSupport::register_type(p, name()) == DDS_RETCODE_OK;
    }
    using DataWriter = P_Alarms_PSM_C_Actual_AlarmDataWriter;
    using DataReader = P_Alarms_PSM_C_Actual_AlarmDataReader;
    using Seq = P_Alarms_PSM_C_Actual_AlarmSeq;
};

}  // namespace rtpdds
#endif
