#pragma once
/**
 * @file Alarms_PSM_enum_utils.hpp
 * @brief Alarms_PSM IDL 기반 enum 타입과 문자열 간 변환을 담당하는 유틸리티.
 *
 * 이 파일은 DDS IDL에서 생성된 Alarms_PSM 관련 enum 타입(`P_Alarms_PSM::T_Actual_Alarm_StateType`)을
 * UI/IPC/JSON 등에서 직관적으로 처리할 수 있도록 문자열 <-> enum 변환 함수를 제공합니다.
 *
 * 연관 파일:
 *   - Alarms_PSM.hpp (IDL에서 생성된 실제 enum 정의)
 *   - sample_factory.[hpp|cpp] (샘플 변환 시 enum ↔ string 변환 활용)
 *   - dds_util.[hpp|cpp] (타입 변환 유틸리티와 연계)
 */
#include <string>
#include "Alarms_PSM.hpp" // 실제 enum 정의가 있는 헤더 또는 IDL에서 생성된 헤더를 include

namespace rtpdds {
namespace P_Alarms_PSM_enum {
    /**
     * @brief enum 값을 문자열로 변환합니다.
     * @param e 변환할 enum 값 (P_Alarms_PSM::T_Actual_Alarm_StateType)
     * @return 해당 enum에 대응하는 문자열(예: "UNACK", "ACK" 등). 알 수 없는 값은 "UNKNOWN" 반환.
     *
     * UI/JSON/IPC 등에서 enum 값을 직관적으로 표현하거나, 역직렬화 시 사용됩니다.
     */
    inline const char* to_string(P_Alarms_PSM::T_Actual_Alarm_StateType e) {
        switch(e){
            case P_Alarms_PSM::T_Actual_Alarm_StateType::L_Actual_Alarm_StateType_Unacknowledged: return "UNACK";
            case P_Alarms_PSM::T_Actual_Alarm_StateType::L_Actual_Alarm_StateType_Acknowledged:   return "ACK";
            case P_Alarms_PSM::T_Actual_Alarm_StateType::L_Actual_Alarm_StateType_Resolved:       return "RESOLVED";
            case P_Alarms_PSM::T_Actual_Alarm_StateType::L_Actual_Alarm_StateType_Destroyed:      return "DESTROYED";
            case P_Alarms_PSM::T_Actual_Alarm_StateType::L_Actual_Alarm_StateType_Cleared:        return "CLEARED";
            default: return "UNKNOWN";
        }
    }

    /**
     * @brief 문자열을 enum 값으로 변환합니다.
     * @param s 변환할 문자열 (예: "UNACK", "ACK" 등)
     * @param out 변환 결과를 저장할 enum 참조
     * @return 변환 성공 시 true, 알 수 없는 문자열이면 false
     *
     * JSON/IPC 등에서 문자열로 전달된 enum 값을 내부 DDS enum으로 변환할 때 사용합니다.
     */
    inline bool from_string(const std::string& s, P_Alarms_PSM::T_Actual_Alarm_StateType& out){
        if(s=="UNACK")    { out=P_Alarms_PSM::T_Actual_Alarm_StateType::L_Actual_Alarm_StateType_Unacknowledged; return true; }
        if(s=="ACK")      { out=P_Alarms_PSM::T_Actual_Alarm_StateType::L_Actual_Alarm_StateType_Acknowledged;   return true; }
        if(s=="RESOLVED") { out=P_Alarms_PSM::T_Actual_Alarm_StateType::L_Actual_Alarm_StateType_Resolved;       return true; }
        if(s=="DESTROYED"){ out=P_Alarms_PSM::T_Actual_Alarm_StateType::L_Actual_Alarm_StateType_Destroyed;      return true; }
        if(s=="CLEARED")  { out=P_Alarms_PSM::T_Actual_Alarm_StateType::L_Actual_Alarm_StateType_Cleared;        return true; }
        return false;
    }
}
}
