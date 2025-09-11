#include "parser_core_gen.hpp"
#include "enum_tables_gen.hpp"
#include <limits>

static inline bool only_keys(const json& j, const std::unordered_set<std::string>& allowed){
    if(!j.is_object()) return false; for(auto it=j.begin(); it!=j.end(); ++it) if(!allowed.count(it.key())) return false; return true;
}
static inline bool chk_i64(const json& v){ return v.is_number_integer(); }
static inline bool chk_num(const json& v){ return v.is_number(); }
static inline bool chk_bool(const json& v){ return v.is_boolean(); }
static inline bool chk_str(const json& v){ return v.is_string(); }
static bool validate_AlarmMsg(const json& in, json& out);
static bool validate_C_Actual_Alarm(const json& in, json& out);
static bool validate_C_Actual_Alarm_Condition(const json& in, json& out);
static bool validate_C_Actual_Alarm_Condition_clearAlarmCondition(const json& in, json& out);
static bool validate_C_Actual_Alarm_Condition_overrideAlarmCondition(const json& in, json& out);
static bool validate_C_Actual_Alarm_Condition_unoverrideAlarmCondition(const json& in, json& out);
static bool validate_C_Actual_Alarm_acknowledgeAlarm(const json& in, json& out);
static bool validate_C_Alarm_Category(const json& in, json& out);
static bool validate_C_Alarm_Category_Specification(const json& in, json& out);
static bool validate_C_Alarm_Condition_Specification(const json& in, json& out);
static bool validate_C_Alarm_Condition_Specification_isOfInterestToCrewRole(const json& in, json& out);
static bool validate_C_Alarm_Condition_Specification_raiseAlarmCondition(const json& in, json& out);
static bool validate_C_Crew_Role_In_Mission_State(const json& in, json& out);
static bool validate_C_Mission_State(const json& in, json& out);
static bool validate_C_Mission_State_setMissionState(const json& in, json& out);
static bool validate_C_Own_Platform(const json& in, json& out);
static bool validate_C_Tone_Specification(const json& in, json& out);
static bool validate_P_Alarms_PSM__C_Actual_Alarm(const json& in, json& out);
static bool validate_P_Alarms_PSM__C_Actual_Alarm_Condition(const json& in, json& out);
static bool validate_P_Alarms_PSM__C_Actual_Alarm_Condition_clearAlarmCondition(const json& in, json& out);
static bool validate_P_Alarms_PSM__C_Actual_Alarm_Condition_overrideAlarmCondition(const json& in, json& out);
static bool validate_P_Alarms_PSM__C_Actual_Alarm_Condition_unoverrideAlarmCondition(const json& in, json& out);
static bool validate_P_Alarms_PSM__C_Actual_Alarm_acknowledgeAlarm(const json& in, json& out);
static bool validate_P_Alarms_PSM__C_Alarm_Category(const json& in, json& out);
static bool validate_P_Alarms_PSM__C_Alarm_Category_Specification(const json& in, json& out);
static bool validate_P_Alarms_PSM__C_Alarm_Condition_Specification(const json& in, json& out);
static bool validate_P_Alarms_PSM__C_Alarm_Condition_Specification_isOfInterestToCrewRole(const json& in, json& out);
static bool validate_P_Alarms_PSM__C_Alarm_Condition_Specification_raiseAlarmCondition(const json& in, json& out);
static bool validate_P_Alarms_PSM__C_Crew_Role_In_Mission_State(const json& in, json& out);
static bool validate_P_Alarms_PSM__C_Mission_State(const json& in, json& out);
static bool validate_P_Alarms_PSM__C_Mission_State_setMissionState(const json& in, json& out);
static bool validate_P_Alarms_PSM__C_Own_Platform(const json& in, json& out);
static bool validate_P_Alarms_PSM__C_Tone_Specification(const json& in, json& out);
static bool validate_P_LDM_Common__T_AngularAcceleration3DType(const json& in, json& out);
static bool validate_P_LDM_Common__T_AngularVelocity3DType(const json& in, json& out);
static bool validate_P_LDM_Common__T_AttitudeType(const json& in, json& out);
static bool validate_P_LDM_Common__T_Coordinate2DType(const json& in, json& out);
static bool validate_P_LDM_Common__T_Coordinate3DType(const json& in, json& out);
static bool validate_P_LDM_Common__T_CoordinatePolar2DType(const json& in, json& out);
static bool validate_P_LDM_Common__T_CoordinatePolar3DType(const json& in, json& out);
static bool validate_P_LDM_Common__T_DateTimeType(const json& in, json& out);
static bool validate_P_LDM_Common__T_DurationType(const json& in, json& out);
static bool validate_P_LDM_Common__T_IdentifierType(const json& in, json& out);
static bool validate_P_LDM_Common__T_LinearAcceleration3DType(const json& in, json& out);
static bool validate_P_LDM_Common__T_LinearOffsetType(const json& in, json& out);
static bool validate_P_LDM_Common__T_LinearSpeed3DType(const json& in, json& out);
static bool validate_P_LDM_Common__T_LinearVelocity2DType(const json& in, json& out);
static bool validate_P_LDM_Common__T_LinearVelocity3DType(const json& in, json& out);
static bool validate_P_LDM_Common__T_PointPolar3DType(const json& in, json& out);
static bool validate_P_LDM_Common__T_Position2DType(const json& in, json& out);
static bool validate_P_LDM_Common__T_RotationalOffsetType(const json& in, json& out);
static bool validate_P_LDM_Common__T_Size2DType(const json& in, json& out);
static bool validate_StringMsg(const json& in, json& out);
static bool validate_T_AngularAcceleration3DType(const json& in, json& out);
static bool validate_T_AngularVelocity3DType(const json& in, json& out);
static bool validate_T_AttitudeType(const json& in, json& out);
static bool validate_T_Coordinate2DType(const json& in, json& out);
static bool validate_T_Coordinate3DType(const json& in, json& out);
static bool validate_T_CoordinatePolar2DType(const json& in, json& out);
static bool validate_T_CoordinatePolar3DType(const json& in, json& out);
static bool validate_T_DateTimeType(const json& in, json& out);
static bool validate_T_DurationType(const json& in, json& out);
static bool validate_T_IdentifierType(const json& in, json& out);
static bool validate_T_LinearAcceleration3DType(const json& in, json& out);
static bool validate_T_LinearOffsetType(const json& in, json& out);
static bool validate_T_LinearSpeed3DType(const json& in, json& out);
static bool validate_T_LinearVelocity2DType(const json& in, json& out);
static bool validate_T_LinearVelocity3DType(const json& in, json& out);
static bool validate_T_PointPolar3DType(const json& in, json& out);
static bool validate_T_Position2DType(const json& in, json& out);
static bool validate_T_RotationalOffsetType(const json& in, json& out);
static bool validate_T_Size2DType(const json& in, json& out);

// validate AlarmMsg
static bool validate_AlarmMsg(const json& in, json& out){
    if(!in.is_object()) return false;
    static const std::unordered_set<std::string> __keys = {"level", "text"};
    if(!only_keys(in, __keys)) return false;
    json o=json::object();
    if(!in.contains("level")) return false;
    if(in.contains("level")) { const auto& v=in["level"]; if(!v.is_number_integer()) return false; o["level"]=v; }
    if(!in.contains("text")) return false;
    if(in.contains("text")) { const auto& v=in["text"]; if(!v.is_string()) return false; std::string s=v.get<std::string>(); o["text"]=std::move(s); }
    out=std::move(o);
    return true;
}

// validate C_Actual_Alarm
static bool validate_C_Actual_Alarm(const json& in, json& out){
    if(!in.is_object()) return false;
    static const std::unordered_set<std::string> __keys = {"A_sourceID", "A_timeOfDataGeneration", "A_componentName", "A_nature", "A_subsystemName", "A_measure", "A_dateTimeRaised", "A_alarmState", "A_raisingCondition_sourceID", "A_alarmCategory_sourceID"};
    if(!only_keys(in, __keys)) return false;
    json o=json::object();
    if(!in.contains("A_sourceID")) return false;
    if(in.contains("A_sourceID")) { const auto& obj=in["A_sourceID"]; json out_child; if(!validate_P_LDM_Common__T_IdentifierType(obj, out_child)) return false; o["A_sourceID"]=std::move(out_child); }
    if(!in.contains("A_timeOfDataGeneration")) return false;
    if(in.contains("A_timeOfDataGeneration")) { const auto& obj=in["A_timeOfDataGeneration"]; json out_child; if(!validate_P_LDM_Common__T_DateTimeType(obj, out_child)) return false; o["A_timeOfDataGeneration"]=std::move(out_child); }
    if(!in.contains("A_componentName")) return false;
    if(in.contains("A_componentName")) { o["A_componentName"]= in["A_componentName"]; }
    if(!in.contains("A_nature")) return false;
    if(in.contains("A_nature")) { o["A_nature"]= in["A_nature"]; }
    if(!in.contains("A_subsystemName")) return false;
    if(in.contains("A_subsystemName")) { o["A_subsystemName"]= in["A_subsystemName"]; }
    if(!in.contains("A_measure")) return false;
    if(in.contains("A_measure")) { o["A_measure"]= in["A_measure"]; }
    if(!in.contains("A_dateTimeRaised")) return false;
    if(in.contains("A_dateTimeRaised")) { const auto& obj=in["A_dateTimeRaised"]; json out_child; if(!validate_P_LDM_Common__T_DateTimeType(obj, out_child)) return false; o["A_dateTimeRaised"]=std::move(out_child); }
    if(!in.contains("A_alarmState")) return false;
    if(in.contains("A_alarmState")) {
        const auto& v=in["A_alarmState"]; if(!v.is_string()) return false;
        std::string tok=v.get<std::string>();
        if(!kEnum_T_Actual_Alarm_StateType.count(tok)) return false;
        o["A_alarmState"]=std::move(tok);
    }
    if(!in.contains("A_raisingCondition_sourceID")) return false;
    if(in.contains("A_raisingCondition_sourceID")) { const auto& obj=in["A_raisingCondition_sourceID"]; json out_child; if(!validate_P_LDM_Common__T_IdentifierType(obj, out_child)) return false; o["A_raisingCondition_sourceID"]=std::move(out_child); }
    if(!in.contains("A_alarmCategory_sourceID")) return false;
    if(in.contains("A_alarmCategory_sourceID")) { const auto& obj=in["A_alarmCategory_sourceID"]; json out_child; if(!validate_P_LDM_Common__T_IdentifierType(obj, out_child)) return false; o["A_alarmCategory_sourceID"]=std::move(out_child); }
    out=std::move(o);
    return true;
}

// validate C_Actual_Alarm_Condition
static bool validate_C_Actual_Alarm_Condition(const json& in, json& out){
    if(!in.is_object()) return false;
    static const std::unordered_set<std::string> __keys = {"A_sourceID", "A_timeOfDataGeneration", "A_alarmSourceID", "A_dateTimeRaised", "A_isOverridden", "A_specification_sourceID", "A_raisedActualAlarm_sourceID"};
    if(!only_keys(in, __keys)) return false;
    json o=json::object();
    if(!in.contains("A_sourceID")) return false;
    if(in.contains("A_sourceID")) { const auto& obj=in["A_sourceID"]; json out_child; if(!validate_P_LDM_Common__T_IdentifierType(obj, out_child)) return false; o["A_sourceID"]=std::move(out_child); }
    if(!in.contains("A_timeOfDataGeneration")) return false;
    if(in.contains("A_timeOfDataGeneration")) { const auto& obj=in["A_timeOfDataGeneration"]; json out_child; if(!validate_P_LDM_Common__T_DateTimeType(obj, out_child)) return false; o["A_timeOfDataGeneration"]=std::move(out_child); }
    if(!in.contains("A_alarmSourceID")) return false;
    if(in.contains("A_alarmSourceID")) { const auto& obj=in["A_alarmSourceID"]; json out_child; if(!validate_P_LDM_Common__T_IdentifierType(obj, out_child)) return false; o["A_alarmSourceID"]=std::move(out_child); }
    if(!in.contains("A_dateTimeRaised")) return false;
    if(in.contains("A_dateTimeRaised")) { const auto& obj=in["A_dateTimeRaised"]; json out_child; if(!validate_P_LDM_Common__T_DateTimeType(obj, out_child)) return false; o["A_dateTimeRaised"]=std::move(out_child); }
    if(!in.contains("A_isOverridden")) return false;
    if(in.contains("A_isOverridden")) { o["A_isOverridden"]= in["A_isOverridden"]; }
    if(!in.contains("A_specification_sourceID")) return false;
    if(in.contains("A_specification_sourceID")) { const auto& obj=in["A_specification_sourceID"]; json out_child; if(!validate_P_LDM_Common__T_IdentifierType(obj, out_child)) return false; o["A_specification_sourceID"]=std::move(out_child); }
    if(!in.contains("A_raisedActualAlarm_sourceID")) return false;
    if(in.contains("A_raisedActualAlarm_sourceID")) { const auto& obj=in["A_raisedActualAlarm_sourceID"]; json out_child; if(!validate_P_LDM_Common__T_IdentifierType(obj, out_child)) return false; o["A_raisedActualAlarm_sourceID"]=std::move(out_child); }
    out=std::move(o);
    return true;
}

// validate C_Actual_Alarm_Condition_clearAlarmCondition
static bool validate_C_Actual_Alarm_Condition_clearAlarmCondition(const json& in, json& out){
    if(!in.is_object()) return false;
    static const std::unordered_set<std::string> __keys = {"A_recipientID", "A_sourceID", "A_referenceNum", "A_timeOfDataGeneration", "A_componentName", "A_subsystemName", "A_measure", "A_nature"};
    if(!only_keys(in, __keys)) return false;
    json o=json::object();
    if(!in.contains("A_recipientID")) return false;
    if(in.contains("A_recipientID")) { const auto& obj=in["A_recipientID"]; json out_child; if(!validate_P_LDM_Common__T_IdentifierType(obj, out_child)) return false; o["A_recipientID"]=std::move(out_child); }
    if(!in.contains("A_sourceID")) return false;
    if(in.contains("A_sourceID")) { const auto& obj=in["A_sourceID"]; json out_child; if(!validate_P_LDM_Common__T_IdentifierType(obj, out_child)) return false; o["A_sourceID"]=std::move(out_child); }
    if(!in.contains("A_referenceNum")) return false;
    if(in.contains("A_referenceNum")) { o["A_referenceNum"]= in["A_referenceNum"]; }
    if(!in.contains("A_timeOfDataGeneration")) return false;
    if(in.contains("A_timeOfDataGeneration")) { const auto& obj=in["A_timeOfDataGeneration"]; json out_child; if(!validate_P_LDM_Common__T_DateTimeType(obj, out_child)) return false; o["A_timeOfDataGeneration"]=std::move(out_child); }
    if(!in.contains("A_componentName")) return false;
    if(in.contains("A_componentName")) { o["A_componentName"]= in["A_componentName"]; }
    if(!in.contains("A_subsystemName")) return false;
    if(in.contains("A_subsystemName")) { o["A_subsystemName"]= in["A_subsystemName"]; }
    if(!in.contains("A_measure")) return false;
    if(in.contains("A_measure")) { o["A_measure"]= in["A_measure"]; }
    if(!in.contains("A_nature")) return false;
    if(in.contains("A_nature")) { o["A_nature"]= in["A_nature"]; }
    out=std::move(o);
    return true;
}

// validate C_Actual_Alarm_Condition_overrideAlarmCondition
static bool validate_C_Actual_Alarm_Condition_overrideAlarmCondition(const json& in, json& out){
    if(!in.is_object()) return false;
    static const std::unordered_set<std::string> __keys = {"A_recipientID", "A_sourceID", "A_referenceNum", "A_timeOfDataGeneration", "A_componentName", "A_subsystemName", "A_measure", "A_nature"};
    if(!only_keys(in, __keys)) return false;
    json o=json::object();
    if(!in.contains("A_recipientID")) return false;
    if(in.contains("A_recipientID")) { const auto& obj=in["A_recipientID"]; json out_child; if(!validate_P_LDM_Common__T_IdentifierType(obj, out_child)) return false; o["A_recipientID"]=std::move(out_child); }
    if(!in.contains("A_sourceID")) return false;
    if(in.contains("A_sourceID")) { const auto& obj=in["A_sourceID"]; json out_child; if(!validate_P_LDM_Common__T_IdentifierType(obj, out_child)) return false; o["A_sourceID"]=std::move(out_child); }
    if(!in.contains("A_referenceNum")) return false;
    if(in.contains("A_referenceNum")) { o["A_referenceNum"]= in["A_referenceNum"]; }
    if(!in.contains("A_timeOfDataGeneration")) return false;
    if(in.contains("A_timeOfDataGeneration")) { const auto& obj=in["A_timeOfDataGeneration"]; json out_child; if(!validate_P_LDM_Common__T_DateTimeType(obj, out_child)) return false; o["A_timeOfDataGeneration"]=std::move(out_child); }
    if(!in.contains("A_componentName")) return false;
    if(in.contains("A_componentName")) { o["A_componentName"]= in["A_componentName"]; }
    if(!in.contains("A_subsystemName")) return false;
    if(in.contains("A_subsystemName")) { o["A_subsystemName"]= in["A_subsystemName"]; }
    if(!in.contains("A_measure")) return false;
    if(in.contains("A_measure")) { o["A_measure"]= in["A_measure"]; }
    if(!in.contains("A_nature")) return false;
    if(in.contains("A_nature")) { o["A_nature"]= in["A_nature"]; }
    out=std::move(o);
    return true;
}

// validate C_Actual_Alarm_Condition_unoverrideAlarmCondition
static bool validate_C_Actual_Alarm_Condition_unoverrideAlarmCondition(const json& in, json& out){
    if(!in.is_object()) return false;
    static const std::unordered_set<std::string> __keys = {"A_recipientID", "A_sourceID", "A_referenceNum", "A_timeOfDataGeneration", "A_componentName", "A_subsystemName", "A_measure", "A_nature"};
    if(!only_keys(in, __keys)) return false;
    json o=json::object();
    if(!in.contains("A_recipientID")) return false;
    if(in.contains("A_recipientID")) { const auto& obj=in["A_recipientID"]; json out_child; if(!validate_P_LDM_Common__T_IdentifierType(obj, out_child)) return false; o["A_recipientID"]=std::move(out_child); }
    if(!in.contains("A_sourceID")) return false;
    if(in.contains("A_sourceID")) { const auto& obj=in["A_sourceID"]; json out_child; if(!validate_P_LDM_Common__T_IdentifierType(obj, out_child)) return false; o["A_sourceID"]=std::move(out_child); }
    if(!in.contains("A_referenceNum")) return false;
    if(in.contains("A_referenceNum")) { o["A_referenceNum"]= in["A_referenceNum"]; }
    if(!in.contains("A_timeOfDataGeneration")) return false;
    if(in.contains("A_timeOfDataGeneration")) { const auto& obj=in["A_timeOfDataGeneration"]; json out_child; if(!validate_P_LDM_Common__T_DateTimeType(obj, out_child)) return false; o["A_timeOfDataGeneration"]=std::move(out_child); }
    if(!in.contains("A_componentName")) return false;
    if(in.contains("A_componentName")) { o["A_componentName"]= in["A_componentName"]; }
    if(!in.contains("A_subsystemName")) return false;
    if(in.contains("A_subsystemName")) { o["A_subsystemName"]= in["A_subsystemName"]; }
    if(!in.contains("A_measure")) return false;
    if(in.contains("A_measure")) { o["A_measure"]= in["A_measure"]; }
    if(!in.contains("A_nature")) return false;
    if(in.contains("A_nature")) { o["A_nature"]= in["A_nature"]; }
    out=std::move(o);
    return true;
}

// validate C_Actual_Alarm_acknowledgeAlarm
static bool validate_C_Actual_Alarm_acknowledgeAlarm(const json& in, json& out){
    if(!in.is_object()) return false;
    static const std::unordered_set<std::string> __keys = {"A_recipientID", "A_sourceID", "A_referenceNum", "A_timeOfDataGeneration", "A_subsystemName", "A_componentName", "A_measure", "A_nature"};
    if(!only_keys(in, __keys)) return false;
    json o=json::object();
    if(!in.contains("A_recipientID")) return false;
    if(in.contains("A_recipientID")) { const auto& obj=in["A_recipientID"]; json out_child; if(!validate_P_LDM_Common__T_IdentifierType(obj, out_child)) return false; o["A_recipientID"]=std::move(out_child); }
    if(!in.contains("A_sourceID")) return false;
    if(in.contains("A_sourceID")) { const auto& obj=in["A_sourceID"]; json out_child; if(!validate_P_LDM_Common__T_IdentifierType(obj, out_child)) return false; o["A_sourceID"]=std::move(out_child); }
    if(!in.contains("A_referenceNum")) return false;
    if(in.contains("A_referenceNum")) { o["A_referenceNum"]= in["A_referenceNum"]; }
    if(!in.contains("A_timeOfDataGeneration")) return false;
    if(in.contains("A_timeOfDataGeneration")) { const auto& obj=in["A_timeOfDataGeneration"]; json out_child; if(!validate_P_LDM_Common__T_DateTimeType(obj, out_child)) return false; o["A_timeOfDataGeneration"]=std::move(out_child); }
    if(!in.contains("A_subsystemName")) return false;
    if(in.contains("A_subsystemName")) { o["A_subsystemName"]= in["A_subsystemName"]; }
    if(!in.contains("A_componentName")) return false;
    if(in.contains("A_componentName")) { o["A_componentName"]= in["A_componentName"]; }
    if(!in.contains("A_measure")) return false;
    if(in.contains("A_measure")) { o["A_measure"]= in["A_measure"]; }
    if(!in.contains("A_nature")) return false;
    if(in.contains("A_nature")) { o["A_nature"]= in["A_nature"]; }
    out=std::move(o);
    return true;
}

// validate C_Alarm_Category
static bool validate_C_Alarm_Category(const json& in, json& out){
    if(!in.is_object()) return false;
    static const std::unordered_set<std::string> __keys = {"A_sourceID", "A_timeOfDataGeneration", "A_activeAlarmCount", "A_unacknowledgedAlarmCount", "A_categorisedActualAlarm_sourceID", "A_alarmCategorySpecification_sourceID"};
    if(!only_keys(in, __keys)) return false;
    json o=json::object();
    if(!in.contains("A_sourceID")) return false;
    if(in.contains("A_sourceID")) { const auto& obj=in["A_sourceID"]; json out_child; if(!validate_P_LDM_Common__T_IdentifierType(obj, out_child)) return false; o["A_sourceID"]=std::move(out_child); }
    if(!in.contains("A_timeOfDataGeneration")) return false;
    if(in.contains("A_timeOfDataGeneration")) { const auto& obj=in["A_timeOfDataGeneration"]; json out_child; if(!validate_P_LDM_Common__T_DateTimeType(obj, out_child)) return false; o["A_timeOfDataGeneration"]=std::move(out_child); }
    if(!in.contains("A_activeAlarmCount")) return false;
    if(in.contains("A_activeAlarmCount")) { o["A_activeAlarmCount"]= in["A_activeAlarmCount"]; }
    if(!in.contains("A_unacknowledgedAlarmCount")) return false;
    if(in.contains("A_unacknowledgedAlarmCount")) { o["A_unacknowledgedAlarmCount"]= in["A_unacknowledgedAlarmCount"]; }
    if(!in.contains("A_categorisedActualAlarm_sourceID")) return false;
    if(in.contains("A_categorisedActualAlarm_sourceID")) { const auto& arr=in["A_categorisedActualAlarm_sourceID"]; if(!arr.is_array()) return false; json a=json::array();
      for(const auto& el: arr){ json ch; if(!validate_P_LDM_Common__T_IdentifierType(el, ch)) return false; a.push_back(std::move(ch)); }
      o["A_categorisedActualAlarm_sourceID"]=std::move(a); }
    if(!in.contains("A_alarmCategorySpecification_sourceID")) return false;
    if(in.contains("A_alarmCategorySpecification_sourceID")) { const auto& obj=in["A_alarmCategorySpecification_sourceID"]; json out_child; if(!validate_P_LDM_Common__T_IdentifierType(obj, out_child)) return false; o["A_alarmCategorySpecification_sourceID"]=std::move(out_child); }
    out=std::move(o);
    return true;
}

// validate C_Alarm_Category_Specification
static bool validate_C_Alarm_Category_Specification(const json& in, json& out){
    if(!in.is_object()) return false;
    static const std::unordered_set<std::string> __keys = {"A_sourceID", "A_timeOfDataGeneration", "A_alarmCategoryName", "A_alarmCategoryAbbreviation", "A_isAutoAcknowledged", "A_automaticAcknowledgeTimeout", "A_hideOnAcknowledge", "A_isRepeated", "A_repeatTimeout", "A_categorisedConditionSpecification_sourceID", "A_notifyingToneSpecification_sourceID", "A_alarmCategory_sourceID"};
    if(!only_keys(in, __keys)) return false;
    json o=json::object();
    if(!in.contains("A_sourceID")) return false;
    if(in.contains("A_sourceID")) { const auto& obj=in["A_sourceID"]; json out_child; if(!validate_P_LDM_Common__T_IdentifierType(obj, out_child)) return false; o["A_sourceID"]=std::move(out_child); }
    if(!in.contains("A_timeOfDataGeneration")) return false;
    if(in.contains("A_timeOfDataGeneration")) { const auto& obj=in["A_timeOfDataGeneration"]; json out_child; if(!validate_P_LDM_Common__T_DateTimeType(obj, out_child)) return false; o["A_timeOfDataGeneration"]=std::move(out_child); }
    if(!in.contains("A_alarmCategoryName")) return false;
    if(in.contains("A_alarmCategoryName")) {
        const auto& v=in["A_alarmCategoryName"]; if(!v.is_string()) return false;
        std::string tok=v.get<std::string>();
        if(!kEnum_T_AlarmCategoryType.count(tok)) return false;
        o["A_alarmCategoryName"]=std::move(tok);
    }
    if(!in.contains("A_alarmCategoryAbbreviation")) return false;
    if(in.contains("A_alarmCategoryAbbreviation")) { o["A_alarmCategoryAbbreviation"]= in["A_alarmCategoryAbbreviation"]; }
    if(!in.contains("A_isAutoAcknowledged")) return false;
    if(in.contains("A_isAutoAcknowledged")) { o["A_isAutoAcknowledged"]= in["A_isAutoAcknowledged"]; }
    if(!in.contains("A_automaticAcknowledgeTimeout")) return false;
    if(in.contains("A_automaticAcknowledgeTimeout")) { const auto& obj=in["A_automaticAcknowledgeTimeout"]; json out_child; if(!validate_P_LDM_Common__T_DurationType(obj, out_child)) return false; o["A_automaticAcknowledgeTimeout"]=std::move(out_child); }
    if(!in.contains("A_hideOnAcknowledge")) return false;
    if(in.contains("A_hideOnAcknowledge")) { o["A_hideOnAcknowledge"]= in["A_hideOnAcknowledge"]; }
    if(!in.contains("A_isRepeated")) return false;
    if(in.contains("A_isRepeated")) { o["A_isRepeated"]= in["A_isRepeated"]; }
    if(!in.contains("A_repeatTimeout")) return false;
    if(in.contains("A_repeatTimeout")) { const auto& obj=in["A_repeatTimeout"]; json out_child; if(!validate_P_LDM_Common__T_DurationType(obj, out_child)) return false; o["A_repeatTimeout"]=std::move(out_child); }
    if(!in.contains("A_categorisedConditionSpecification_sourceID")) return false;
    if(in.contains("A_categorisedConditionSpecification_sourceID")) { const auto& arr=in["A_categorisedConditionSpecification_sourceID"]; if(!arr.is_array()) return false; json a=json::array();
      for(const auto& el: arr){ json ch; if(!validate_P_LDM_Common__T_IdentifierType(el, ch)) return false; a.push_back(std::move(ch)); }
      o["A_categorisedConditionSpecification_sourceID"]=std::move(a); }
    if(!in.contains("A_notifyingToneSpecification_sourceID")) return false;
    if(in.contains("A_notifyingToneSpecification_sourceID")) { const auto& obj=in["A_notifyingToneSpecification_sourceID"]; json out_child; if(!validate_P_LDM_Common__T_IdentifierType(obj, out_child)) return false; o["A_notifyingToneSpecification_sourceID"]=std::move(out_child); }
    if(!in.contains("A_alarmCategory_sourceID")) return false;
    if(in.contains("A_alarmCategory_sourceID")) { const auto& obj=in["A_alarmCategory_sourceID"]; json out_child; if(!validate_P_LDM_Common__T_IdentifierType(obj, out_child)) return false; o["A_alarmCategory_sourceID"]=std::move(out_child); }
    out=std::move(o);
    return true;
}

// validate C_Alarm_Condition_Specification
static bool validate_C_Alarm_Condition_Specification(const json& in, json& out){
    if(!in.is_object()) return false;
    static const std::unordered_set<std::string> __keys = {"A_sourceID", "A_timeOfDataGeneration", "A_subsystemName", "A_componentName", "A_measure", "A_nature", "A_alarmConditionCategory", "A_alarmConditionName", "A_hasMultipleInstances", "A_overrideState", "A_actualAlarmCondition_sourceID", "A_alarmCategory_sourceID", "A_interestedRole_sourceID"};
    if(!only_keys(in, __keys)) return false;
    json o=json::object();
    if(!in.contains("A_sourceID")) return false;
    if(in.contains("A_sourceID")) { const auto& obj=in["A_sourceID"]; json out_child; if(!validate_P_LDM_Common__T_IdentifierType(obj, out_child)) return false; o["A_sourceID"]=std::move(out_child); }
    if(!in.contains("A_timeOfDataGeneration")) return false;
    if(in.contains("A_timeOfDataGeneration")) { const auto& obj=in["A_timeOfDataGeneration"]; json out_child; if(!validate_P_LDM_Common__T_DateTimeType(obj, out_child)) return false; o["A_timeOfDataGeneration"]=std::move(out_child); }
    if(!in.contains("A_subsystemName")) return false;
    if(in.contains("A_subsystemName")) { o["A_subsystemName"]= in["A_subsystemName"]; }
    if(!in.contains("A_componentName")) return false;
    if(in.contains("A_componentName")) { o["A_componentName"]= in["A_componentName"]; }
    if(!in.contains("A_measure")) return false;
    if(in.contains("A_measure")) { o["A_measure"]= in["A_measure"]; }
    if(!in.contains("A_nature")) return false;
    if(in.contains("A_nature")) { o["A_nature"]= in["A_nature"]; }
    if(!in.contains("A_alarmConditionCategory")) return false;
    if(in.contains("A_alarmConditionCategory")) { o["A_alarmConditionCategory"]= in["A_alarmConditionCategory"]; }
    if(!in.contains("A_alarmConditionName")) return false;
    if(in.contains("A_alarmConditionName")) { o["A_alarmConditionName"]= in["A_alarmConditionName"]; }
    if(!in.contains("A_hasMultipleInstances")) return false;
    if(in.contains("A_hasMultipleInstances")) { o["A_hasMultipleInstances"]= in["A_hasMultipleInstances"]; }
    if(!in.contains("A_overrideState")) return false;
    if(in.contains("A_overrideState")) {
        const auto& v=in["A_overrideState"]; if(!v.is_string()) return false;
        std::string tok=v.get<std::string>();
        if(!kEnum_T_Alarm_Condition_Specification_StateType.count(tok)) return false;
        o["A_overrideState"]=std::move(tok);
    }
    if(!in.contains("A_actualAlarmCondition_sourceID")) return false;
    if(in.contains("A_actualAlarmCondition_sourceID")) { const auto& arr=in["A_actualAlarmCondition_sourceID"]; if(!arr.is_array()) return false; json a=json::array();
      for(const auto& el: arr){ json ch; if(!validate_P_LDM_Common__T_IdentifierType(el, ch)) return false; a.push_back(std::move(ch)); }
      o["A_actualAlarmCondition_sourceID"]=std::move(a); }
    if(!in.contains("A_alarmCategory_sourceID")) return false;
    if(in.contains("A_alarmCategory_sourceID")) { const auto& obj=in["A_alarmCategory_sourceID"]; json out_child; if(!validate_P_LDM_Common__T_IdentifierType(obj, out_child)) return false; o["A_alarmCategory_sourceID"]=std::move(out_child); }
    if(!in.contains("A_interestedRole_sourceID")) return false;
    if(in.contains("A_interestedRole_sourceID")) { const auto& arr=in["A_interestedRole_sourceID"]; if(!arr.is_array()) return false; json a=json::array();
      for(const auto& el: arr){ json ch; if(!validate_P_LDM_Common__T_IdentifierType(el, ch)) return false; a.push_back(std::move(ch)); }
      o["A_interestedRole_sourceID"]=std::move(a); }
    out=std::move(o);
    return true;
}

// validate C_Alarm_Condition_Specification_isOfInterestToCrewRole
static bool validate_C_Alarm_Condition_Specification_isOfInterestToCrewRole(const json& in, json& out){
    if(!in.is_object()) return false;
    static const std::unordered_set<std::string> __keys = {"A_recipientID", "A_sourceID", "A_referenceNum", "A_timeOfDataGeneration", "A_crewRole"};
    if(!only_keys(in, __keys)) return false;
    json o=json::object();
    if(!in.contains("A_recipientID")) return false;
    if(in.contains("A_recipientID")) { const auto& obj=in["A_recipientID"]; json out_child; if(!validate_P_LDM_Common__T_IdentifierType(obj, out_child)) return false; o["A_recipientID"]=std::move(out_child); }
    if(!in.contains("A_sourceID")) return false;
    if(in.contains("A_sourceID")) { const auto& obj=in["A_sourceID"]; json out_child; if(!validate_P_LDM_Common__T_IdentifierType(obj, out_child)) return false; o["A_sourceID"]=std::move(out_child); }
    if(!in.contains("A_referenceNum")) return false;
    if(in.contains("A_referenceNum")) { o["A_referenceNum"]= in["A_referenceNum"]; }
    if(!in.contains("A_timeOfDataGeneration")) return false;
    if(in.contains("A_timeOfDataGeneration")) { const auto& obj=in["A_timeOfDataGeneration"]; json out_child; if(!validate_P_LDM_Common__T_DateTimeType(obj, out_child)) return false; o["A_timeOfDataGeneration"]=std::move(out_child); }
    if(!in.contains("A_crewRole")) return false;
    if(in.contains("A_crewRole")) { o["A_crewRole"]= in["A_crewRole"]; }
    out=std::move(o);
    return true;
}

// validate C_Alarm_Condition_Specification_raiseAlarmCondition
static bool validate_C_Alarm_Condition_Specification_raiseAlarmCondition(const json& in, json& out){
    if(!in.is_object()) return false;
    static const std::unordered_set<std::string> __keys = {"A_recipientID", "A_sourceID", "A_referenceNum", "A_timeOfDataGeneration", "A_componentName", "A_subsystemName", "A_measure", "A_nature"};
    if(!only_keys(in, __keys)) return false;
    json o=json::object();
    if(!in.contains("A_recipientID")) return false;
    if(in.contains("A_recipientID")) { const auto& obj=in["A_recipientID"]; json out_child; if(!validate_P_LDM_Common__T_IdentifierType(obj, out_child)) return false; o["A_recipientID"]=std::move(out_child); }
    if(!in.contains("A_sourceID")) return false;
    if(in.contains("A_sourceID")) { const auto& obj=in["A_sourceID"]; json out_child; if(!validate_P_LDM_Common__T_IdentifierType(obj, out_child)) return false; o["A_sourceID"]=std::move(out_child); }
    if(!in.contains("A_referenceNum")) return false;
    if(in.contains("A_referenceNum")) { o["A_referenceNum"]= in["A_referenceNum"]; }
    if(!in.contains("A_timeOfDataGeneration")) return false;
    if(in.contains("A_timeOfDataGeneration")) { const auto& obj=in["A_timeOfDataGeneration"]; json out_child; if(!validate_P_LDM_Common__T_DateTimeType(obj, out_child)) return false; o["A_timeOfDataGeneration"]=std::move(out_child); }
    if(!in.contains("A_componentName")) return false;
    if(in.contains("A_componentName")) { o["A_componentName"]= in["A_componentName"]; }
    if(!in.contains("A_subsystemName")) return false;
    if(in.contains("A_subsystemName")) { o["A_subsystemName"]= in["A_subsystemName"]; }
    if(!in.contains("A_measure")) return false;
    if(in.contains("A_measure")) { o["A_measure"]= in["A_measure"]; }
    if(!in.contains("A_nature")) return false;
    if(in.contains("A_nature")) { o["A_nature"]= in["A_nature"]; }
    out=std::move(o);
    return true;
}

// validate C_Crew_Role_In_Mission_State
static bool validate_C_Crew_Role_In_Mission_State(const json& in, json& out){
    if(!in.is_object()) return false;
    static const std::unordered_set<std::string> __keys = {"A_sourceID", "A_timeOfDataGeneration", "A_crewRoleName", "A_relevantAlarmType_sourceID", "A_missionState_sourceID"};
    if(!only_keys(in, __keys)) return false;
    json o=json::object();
    if(!in.contains("A_sourceID")) return false;
    if(in.contains("A_sourceID")) { const auto& obj=in["A_sourceID"]; json out_child; if(!validate_P_LDM_Common__T_IdentifierType(obj, out_child)) return false; o["A_sourceID"]=std::move(out_child); }
    if(!in.contains("A_timeOfDataGeneration")) return false;
    if(in.contains("A_timeOfDataGeneration")) { const auto& obj=in["A_timeOfDataGeneration"]; json out_child; if(!validate_P_LDM_Common__T_DateTimeType(obj, out_child)) return false; o["A_timeOfDataGeneration"]=std::move(out_child); }
    if(!in.contains("A_crewRoleName")) return false;
    if(in.contains("A_crewRoleName")) { o["A_crewRoleName"]= in["A_crewRoleName"]; }
    if(!in.contains("A_relevantAlarmType_sourceID")) return false;
    if(in.contains("A_relevantAlarmType_sourceID")) { const auto& arr=in["A_relevantAlarmType_sourceID"]; if(!arr.is_array()) return false; json a=json::array();
      for(const auto& el: arr){ json ch; if(!validate_P_LDM_Common__T_IdentifierType(el, ch)) return false; a.push_back(std::move(ch)); }
      o["A_relevantAlarmType_sourceID"]=std::move(a); }
    if(!in.contains("A_missionState_sourceID")) return false;
    if(in.contains("A_missionState_sourceID")) { const auto& arr=in["A_missionState_sourceID"]; if(!arr.is_array()) return false; json a=json::array();
      for(const auto& el: arr){ json ch; if(!validate_P_LDM_Common__T_IdentifierType(el, ch)) return false; a.push_back(std::move(ch)); }
      o["A_missionState_sourceID"]=std::move(a); }
    out=std::move(o);
    return true;
}

// validate C_Mission_State
static bool validate_C_Mission_State(const json& in, json& out){
    if(!in.is_object()) return false;
    static const std::unordered_set<std::string> __keys = {"A_sourceID", "A_timeOfDataGeneration", "A_missionState", "A_missionStateName", "A_crewRoleInMissionState_sourceID", "A_ownPlatform_sourceID"};
    if(!only_keys(in, __keys)) return false;
    json o=json::object();
    if(!in.contains("A_sourceID")) return false;
    if(in.contains("A_sourceID")) { const auto& obj=in["A_sourceID"]; json out_child; if(!validate_P_LDM_Common__T_IdentifierType(obj, out_child)) return false; o["A_sourceID"]=std::move(out_child); }
    if(!in.contains("A_timeOfDataGeneration")) return false;
    if(in.contains("A_timeOfDataGeneration")) { const auto& obj=in["A_timeOfDataGeneration"]; json out_child; if(!validate_P_LDM_Common__T_DateTimeType(obj, out_child)) return false; o["A_timeOfDataGeneration"]=std::move(out_child); }
    if(!in.contains("A_missionState")) return false;
    if(in.contains("A_missionState")) {
        const auto& v=in["A_missionState"]; if(!v.is_string()) return false;
        std::string tok=v.get<std::string>();
        if(!kEnum_T_MissionStateType.count(tok)) return false;
        o["A_missionState"]=std::move(tok);
    }
    if(!in.contains("A_missionStateName")) return false;
    if(in.contains("A_missionStateName")) { o["A_missionStateName"]= in["A_missionStateName"]; }
    if(!in.contains("A_crewRoleInMissionState_sourceID")) return false;
    if(in.contains("A_crewRoleInMissionState_sourceID")) { const auto& arr=in["A_crewRoleInMissionState_sourceID"]; if(!arr.is_array()) return false; json a=json::array();
      for(const auto& el: arr){ json ch; if(!validate_P_LDM_Common__T_IdentifierType(el, ch)) return false; a.push_back(std::move(ch)); }
      o["A_crewRoleInMissionState_sourceID"]=std::move(a); }
    if(!in.contains("A_ownPlatform_sourceID")) return false;
    if(in.contains("A_ownPlatform_sourceID")) { const auto& obj=in["A_ownPlatform_sourceID"]; json out_child; if(!validate_P_LDM_Common__T_IdentifierType(obj, out_child)) return false; o["A_ownPlatform_sourceID"]=std::move(out_child); }
    out=std::move(o);
    return true;
}

// validate C_Mission_State_setMissionState
static bool validate_C_Mission_State_setMissionState(const json& in, json& out){
    if(!in.is_object()) return false;
    static const std::unordered_set<std::string> __keys = {"A_recipientID", "A_sourceID", "A_referenceNum", "A_timeOfDataGeneration", "A_missionState", "A_missionStateName"};
    if(!only_keys(in, __keys)) return false;
    json o=json::object();
    if(!in.contains("A_recipientID")) return false;
    if(in.contains("A_recipientID")) { const auto& obj=in["A_recipientID"]; json out_child; if(!validate_P_LDM_Common__T_IdentifierType(obj, out_child)) return false; o["A_recipientID"]=std::move(out_child); }
    if(!in.contains("A_sourceID")) return false;
    if(in.contains("A_sourceID")) { const auto& obj=in["A_sourceID"]; json out_child; if(!validate_P_LDM_Common__T_IdentifierType(obj, out_child)) return false; o["A_sourceID"]=std::move(out_child); }
    if(!in.contains("A_referenceNum")) return false;
    if(in.contains("A_referenceNum")) { o["A_referenceNum"]= in["A_referenceNum"]; }
    if(!in.contains("A_timeOfDataGeneration")) return false;
    if(in.contains("A_timeOfDataGeneration")) { const auto& obj=in["A_timeOfDataGeneration"]; json out_child; if(!validate_P_LDM_Common__T_DateTimeType(obj, out_child)) return false; o["A_timeOfDataGeneration"]=std::move(out_child); }
    if(!in.contains("A_missionState")) return false;
    if(in.contains("A_missionState")) {
        const auto& v=in["A_missionState"]; if(!v.is_string()) return false;
        std::string tok=v.get<std::string>();
        if(!kEnum_T_MissionStateType.count(tok)) return false;
        o["A_missionState"]=std::move(tok);
    }
    if(!in.contains("A_missionStateName")) return false;
    if(in.contains("A_missionStateName")) { o["A_missionStateName"]= in["A_missionStateName"]; }
    out=std::move(o);
    return true;
}

// validate C_Own_Platform
static bool validate_C_Own_Platform(const json& in, json& out){
    if(!in.is_object()) return false;
    static const std::unordered_set<std::string> __keys = {"A_sourceID", "A_timeOfDataGeneration", "A_activeAlarmsExist", "A_possibleMissionState_sourceID"};
    if(!only_keys(in, __keys)) return false;
    json o=json::object();
    if(!in.contains("A_sourceID")) return false;
    if(in.contains("A_sourceID")) { const auto& obj=in["A_sourceID"]; json out_child; if(!validate_P_LDM_Common__T_IdentifierType(obj, out_child)) return false; o["A_sourceID"]=std::move(out_child); }
    if(!in.contains("A_timeOfDataGeneration")) return false;
    if(in.contains("A_timeOfDataGeneration")) { const auto& obj=in["A_timeOfDataGeneration"]; json out_child; if(!validate_P_LDM_Common__T_DateTimeType(obj, out_child)) return false; o["A_timeOfDataGeneration"]=std::move(out_child); }
    if(!in.contains("A_activeAlarmsExist")) return false;
    if(in.contains("A_activeAlarmsExist")) { o["A_activeAlarmsExist"]= in["A_activeAlarmsExist"]; }
    if(!in.contains("A_possibleMissionState_sourceID")) return false;
    if(in.contains("A_possibleMissionState_sourceID")) { const auto& arr=in["A_possibleMissionState_sourceID"]; if(!arr.is_array()) return false; json a=json::array();
      for(const auto& el: arr){ json ch; if(!validate_P_LDM_Common__T_IdentifierType(el, ch)) return false; a.push_back(std::move(ch)); }
      o["A_possibleMissionState_sourceID"]=std::move(a); }
    out=std::move(o);
    return true;
}

// validate C_Tone_Specification
static bool validate_C_Tone_Specification(const json& in, json& out){
    if(!in.is_object()) return false;
    static const std::unordered_set<std::string> __keys = {"A_sourceID", "A_timeOfDataGeneration", "A_toneFrequency", "A_toneModulationType", "A_toneRepetitionFrequency", "A_toneMaxVolume", "A_alarmCategorySpecification_sourceID"};
    if(!only_keys(in, __keys)) return false;
    json o=json::object();
    if(!in.contains("A_sourceID")) return false;
    if(in.contains("A_sourceID")) { const auto& obj=in["A_sourceID"]; json out_child; if(!validate_P_LDM_Common__T_IdentifierType(obj, out_child)) return false; o["A_sourceID"]=std::move(out_child); }
    if(!in.contains("A_timeOfDataGeneration")) return false;
    if(in.contains("A_timeOfDataGeneration")) { const auto& obj=in["A_timeOfDataGeneration"]; json out_child; if(!validate_P_LDM_Common__T_DateTimeType(obj, out_child)) return false; o["A_timeOfDataGeneration"]=std::move(out_child); }
    if(!in.contains("A_toneFrequency")) return false;
    if(in.contains("A_toneFrequency")) { o["A_toneFrequency"]= in["A_toneFrequency"]; }
    if(!in.contains("A_toneModulationType")) return false;
    if(in.contains("A_toneModulationType")) { o["A_toneModulationType"]= in["A_toneModulationType"]; }
    if(!in.contains("A_toneRepetitionFrequency")) return false;
    if(in.contains("A_toneRepetitionFrequency")) { o["A_toneRepetitionFrequency"]= in["A_toneRepetitionFrequency"]; }
    if(!in.contains("A_toneMaxVolume")) return false;
    if(in.contains("A_toneMaxVolume")) { o["A_toneMaxVolume"]= in["A_toneMaxVolume"]; }
    if(!in.contains("A_alarmCategorySpecification_sourceID")) return false;
    if(in.contains("A_alarmCategorySpecification_sourceID")) { const auto& obj=in["A_alarmCategorySpecification_sourceID"]; json out_child; if(!validate_P_LDM_Common__T_IdentifierType(obj, out_child)) return false; o["A_alarmCategorySpecification_sourceID"]=std::move(out_child); }
    out=std::move(o);
    return true;
}

// validate P_Alarms_PSM::C_Actual_Alarm
static bool validate_P_Alarms_PSM__C_Actual_Alarm(const json& in, json& out){
    if(!in.is_object()) return false;
    static const std::unordered_set<std::string> __keys = {"A_sourceID", "A_timeOfDataGeneration", "A_componentName", "A_nature", "A_subsystemName", "A_measure", "A_dateTimeRaised", "A_alarmState", "A_raisingCondition_sourceID", "A_alarmCategory_sourceID"};
    if(!only_keys(in, __keys)) return false;
    json o=json::object();
    if(!in.contains("A_sourceID")) return false;
    if(in.contains("A_sourceID")) { const auto& obj=in["A_sourceID"]; json out_child; if(!validate_P_LDM_Common__T_IdentifierType(obj, out_child)) return false; o["A_sourceID"]=std::move(out_child); }
    if(!in.contains("A_timeOfDataGeneration")) return false;
    if(in.contains("A_timeOfDataGeneration")) { const auto& obj=in["A_timeOfDataGeneration"]; json out_child; if(!validate_P_LDM_Common__T_DateTimeType(obj, out_child)) return false; o["A_timeOfDataGeneration"]=std::move(out_child); }
    if(!in.contains("A_componentName")) return false;
    if(in.contains("A_componentName")) { o["A_componentName"]= in["A_componentName"]; }
    if(!in.contains("A_nature")) return false;
    if(in.contains("A_nature")) { o["A_nature"]= in["A_nature"]; }
    if(!in.contains("A_subsystemName")) return false;
    if(in.contains("A_subsystemName")) { o["A_subsystemName"]= in["A_subsystemName"]; }
    if(!in.contains("A_measure")) return false;
    if(in.contains("A_measure")) { o["A_measure"]= in["A_measure"]; }
    if(!in.contains("A_dateTimeRaised")) return false;
    if(in.contains("A_dateTimeRaised")) { const auto& obj=in["A_dateTimeRaised"]; json out_child; if(!validate_P_LDM_Common__T_DateTimeType(obj, out_child)) return false; o["A_dateTimeRaised"]=std::move(out_child); }
    if(!in.contains("A_alarmState")) return false;
    if(in.contains("A_alarmState")) {
        const auto& v=in["A_alarmState"]; if(!v.is_string()) return false;
        std::string tok=v.get<std::string>();
        if(!kEnum_T_Actual_Alarm_StateType.count(tok)) return false;
        o["A_alarmState"]=std::move(tok);
    }
    if(!in.contains("A_raisingCondition_sourceID")) return false;
    if(in.contains("A_raisingCondition_sourceID")) { const auto& obj=in["A_raisingCondition_sourceID"]; json out_child; if(!validate_P_LDM_Common__T_IdentifierType(obj, out_child)) return false; o["A_raisingCondition_sourceID"]=std::move(out_child); }
    if(!in.contains("A_alarmCategory_sourceID")) return false;
    if(in.contains("A_alarmCategory_sourceID")) { const auto& obj=in["A_alarmCategory_sourceID"]; json out_child; if(!validate_P_LDM_Common__T_IdentifierType(obj, out_child)) return false; o["A_alarmCategory_sourceID"]=std::move(out_child); }
    out=std::move(o);
    return true;
}

// validate P_Alarms_PSM::C_Actual_Alarm_Condition
static bool validate_P_Alarms_PSM__C_Actual_Alarm_Condition(const json& in, json& out){
    if(!in.is_object()) return false;
    static const std::unordered_set<std::string> __keys = {"A_sourceID", "A_timeOfDataGeneration", "A_alarmSourceID", "A_dateTimeRaised", "A_isOverridden", "A_specification_sourceID", "A_raisedActualAlarm_sourceID"};
    if(!only_keys(in, __keys)) return false;
    json o=json::object();
    if(!in.contains("A_sourceID")) return false;
    if(in.contains("A_sourceID")) { const auto& obj=in["A_sourceID"]; json out_child; if(!validate_P_LDM_Common__T_IdentifierType(obj, out_child)) return false; o["A_sourceID"]=std::move(out_child); }
    if(!in.contains("A_timeOfDataGeneration")) return false;
    if(in.contains("A_timeOfDataGeneration")) { const auto& obj=in["A_timeOfDataGeneration"]; json out_child; if(!validate_P_LDM_Common__T_DateTimeType(obj, out_child)) return false; o["A_timeOfDataGeneration"]=std::move(out_child); }
    if(!in.contains("A_alarmSourceID")) return false;
    if(in.contains("A_alarmSourceID")) { const auto& obj=in["A_alarmSourceID"]; json out_child; if(!validate_P_LDM_Common__T_IdentifierType(obj, out_child)) return false; o["A_alarmSourceID"]=std::move(out_child); }
    if(!in.contains("A_dateTimeRaised")) return false;
    if(in.contains("A_dateTimeRaised")) { const auto& obj=in["A_dateTimeRaised"]; json out_child; if(!validate_P_LDM_Common__T_DateTimeType(obj, out_child)) return false; o["A_dateTimeRaised"]=std::move(out_child); }
    if(!in.contains("A_isOverridden")) return false;
    if(in.contains("A_isOverridden")) { o["A_isOverridden"]= in["A_isOverridden"]; }
    if(!in.contains("A_specification_sourceID")) return false;
    if(in.contains("A_specification_sourceID")) { const auto& obj=in["A_specification_sourceID"]; json out_child; if(!validate_P_LDM_Common__T_IdentifierType(obj, out_child)) return false; o["A_specification_sourceID"]=std::move(out_child); }
    if(!in.contains("A_raisedActualAlarm_sourceID")) return false;
    if(in.contains("A_raisedActualAlarm_sourceID")) { const auto& obj=in["A_raisedActualAlarm_sourceID"]; json out_child; if(!validate_P_LDM_Common__T_IdentifierType(obj, out_child)) return false; o["A_raisedActualAlarm_sourceID"]=std::move(out_child); }
    out=std::move(o);
    return true;
}

// validate P_Alarms_PSM::C_Actual_Alarm_Condition_clearAlarmCondition
static bool validate_P_Alarms_PSM__C_Actual_Alarm_Condition_clearAlarmCondition(const json& in, json& out){
    if(!in.is_object()) return false;
    static const std::unordered_set<std::string> __keys = {"A_recipientID", "A_sourceID", "A_referenceNum", "A_timeOfDataGeneration", "A_componentName", "A_subsystemName", "A_measure", "A_nature"};
    if(!only_keys(in, __keys)) return false;
    json o=json::object();
    if(!in.contains("A_recipientID")) return false;
    if(in.contains("A_recipientID")) { const auto& obj=in["A_recipientID"]; json out_child; if(!validate_P_LDM_Common__T_IdentifierType(obj, out_child)) return false; o["A_recipientID"]=std::move(out_child); }
    if(!in.contains("A_sourceID")) return false;
    if(in.contains("A_sourceID")) { const auto& obj=in["A_sourceID"]; json out_child; if(!validate_P_LDM_Common__T_IdentifierType(obj, out_child)) return false; o["A_sourceID"]=std::move(out_child); }
    if(!in.contains("A_referenceNum")) return false;
    if(in.contains("A_referenceNum")) { o["A_referenceNum"]= in["A_referenceNum"]; }
    if(!in.contains("A_timeOfDataGeneration")) return false;
    if(in.contains("A_timeOfDataGeneration")) { const auto& obj=in["A_timeOfDataGeneration"]; json out_child; if(!validate_P_LDM_Common__T_DateTimeType(obj, out_child)) return false; o["A_timeOfDataGeneration"]=std::move(out_child); }
    if(!in.contains("A_componentName")) return false;
    if(in.contains("A_componentName")) { o["A_componentName"]= in["A_componentName"]; }
    if(!in.contains("A_subsystemName")) return false;
    if(in.contains("A_subsystemName")) { o["A_subsystemName"]= in["A_subsystemName"]; }
    if(!in.contains("A_measure")) return false;
    if(in.contains("A_measure")) { o["A_measure"]= in["A_measure"]; }
    if(!in.contains("A_nature")) return false;
    if(in.contains("A_nature")) { o["A_nature"]= in["A_nature"]; }
    out=std::move(o);
    return true;
}

// validate P_Alarms_PSM::C_Actual_Alarm_Condition_overrideAlarmCondition
static bool validate_P_Alarms_PSM__C_Actual_Alarm_Condition_overrideAlarmCondition(const json& in, json& out){
    if(!in.is_object()) return false;
    static const std::unordered_set<std::string> __keys = {"A_recipientID", "A_sourceID", "A_referenceNum", "A_timeOfDataGeneration", "A_componentName", "A_subsystemName", "A_measure", "A_nature"};
    if(!only_keys(in, __keys)) return false;
    json o=json::object();
    if(!in.contains("A_recipientID")) return false;
    if(in.contains("A_recipientID")) { const auto& obj=in["A_recipientID"]; json out_child; if(!validate_P_LDM_Common__T_IdentifierType(obj, out_child)) return false; o["A_recipientID"]=std::move(out_child); }
    if(!in.contains("A_sourceID")) return false;
    if(in.contains("A_sourceID")) { const auto& obj=in["A_sourceID"]; json out_child; if(!validate_P_LDM_Common__T_IdentifierType(obj, out_child)) return false; o["A_sourceID"]=std::move(out_child); }
    if(!in.contains("A_referenceNum")) return false;
    if(in.contains("A_referenceNum")) { o["A_referenceNum"]= in["A_referenceNum"]; }
    if(!in.contains("A_timeOfDataGeneration")) return false;
    if(in.contains("A_timeOfDataGeneration")) { const auto& obj=in["A_timeOfDataGeneration"]; json out_child; if(!validate_P_LDM_Common__T_DateTimeType(obj, out_child)) return false; o["A_timeOfDataGeneration"]=std::move(out_child); }
    if(!in.contains("A_componentName")) return false;
    if(in.contains("A_componentName")) { o["A_componentName"]= in["A_componentName"]; }
    if(!in.contains("A_subsystemName")) return false;
    if(in.contains("A_subsystemName")) { o["A_subsystemName"]= in["A_subsystemName"]; }
    if(!in.contains("A_measure")) return false;
    if(in.contains("A_measure")) { o["A_measure"]= in["A_measure"]; }
    if(!in.contains("A_nature")) return false;
    if(in.contains("A_nature")) { o["A_nature"]= in["A_nature"]; }
    out=std::move(o);
    return true;
}

// validate P_Alarms_PSM::C_Actual_Alarm_Condition_unoverrideAlarmCondition
static bool validate_P_Alarms_PSM__C_Actual_Alarm_Condition_unoverrideAlarmCondition(const json& in, json& out){
    if(!in.is_object()) return false;
    static const std::unordered_set<std::string> __keys = {"A_recipientID", "A_sourceID", "A_referenceNum", "A_timeOfDataGeneration", "A_componentName", "A_subsystemName", "A_measure", "A_nature"};
    if(!only_keys(in, __keys)) return false;
    json o=json::object();
    if(!in.contains("A_recipientID")) return false;
    if(in.contains("A_recipientID")) { const auto& obj=in["A_recipientID"]; json out_child; if(!validate_P_LDM_Common__T_IdentifierType(obj, out_child)) return false; o["A_recipientID"]=std::move(out_child); }
    if(!in.contains("A_sourceID")) return false;
    if(in.contains("A_sourceID")) { const auto& obj=in["A_sourceID"]; json out_child; if(!validate_P_LDM_Common__T_IdentifierType(obj, out_child)) return false; o["A_sourceID"]=std::move(out_child); }
    if(!in.contains("A_referenceNum")) return false;
    if(in.contains("A_referenceNum")) { o["A_referenceNum"]= in["A_referenceNum"]; }
    if(!in.contains("A_timeOfDataGeneration")) return false;
    if(in.contains("A_timeOfDataGeneration")) { const auto& obj=in["A_timeOfDataGeneration"]; json out_child; if(!validate_P_LDM_Common__T_DateTimeType(obj, out_child)) return false; o["A_timeOfDataGeneration"]=std::move(out_child); }
    if(!in.contains("A_componentName")) return false;
    if(in.contains("A_componentName")) { o["A_componentName"]= in["A_componentName"]; }
    if(!in.contains("A_subsystemName")) return false;
    if(in.contains("A_subsystemName")) { o["A_subsystemName"]= in["A_subsystemName"]; }
    if(!in.contains("A_measure")) return false;
    if(in.contains("A_measure")) { o["A_measure"]= in["A_measure"]; }
    if(!in.contains("A_nature")) return false;
    if(in.contains("A_nature")) { o["A_nature"]= in["A_nature"]; }
    out=std::move(o);
    return true;
}

// validate P_Alarms_PSM::C_Actual_Alarm_acknowledgeAlarm
static bool validate_P_Alarms_PSM__C_Actual_Alarm_acknowledgeAlarm(const json& in, json& out){
    if(!in.is_object()) return false;
    static const std::unordered_set<std::string> __keys = {"A_recipientID", "A_sourceID", "A_referenceNum", "A_timeOfDataGeneration", "A_subsystemName", "A_componentName", "A_measure", "A_nature"};
    if(!only_keys(in, __keys)) return false;
    json o=json::object();
    if(!in.contains("A_recipientID")) return false;
    if(in.contains("A_recipientID")) { const auto& obj=in["A_recipientID"]; json out_child; if(!validate_P_LDM_Common__T_IdentifierType(obj, out_child)) return false; o["A_recipientID"]=std::move(out_child); }
    if(!in.contains("A_sourceID")) return false;
    if(in.contains("A_sourceID")) { const auto& obj=in["A_sourceID"]; json out_child; if(!validate_P_LDM_Common__T_IdentifierType(obj, out_child)) return false; o["A_sourceID"]=std::move(out_child); }
    if(!in.contains("A_referenceNum")) return false;
    if(in.contains("A_referenceNum")) { o["A_referenceNum"]= in["A_referenceNum"]; }
    if(!in.contains("A_timeOfDataGeneration")) return false;
    if(in.contains("A_timeOfDataGeneration")) { const auto& obj=in["A_timeOfDataGeneration"]; json out_child; if(!validate_P_LDM_Common__T_DateTimeType(obj, out_child)) return false; o["A_timeOfDataGeneration"]=std::move(out_child); }
    if(!in.contains("A_subsystemName")) return false;
    if(in.contains("A_subsystemName")) { o["A_subsystemName"]= in["A_subsystemName"]; }
    if(!in.contains("A_componentName")) return false;
    if(in.contains("A_componentName")) { o["A_componentName"]= in["A_componentName"]; }
    if(!in.contains("A_measure")) return false;
    if(in.contains("A_measure")) { o["A_measure"]= in["A_measure"]; }
    if(!in.contains("A_nature")) return false;
    if(in.contains("A_nature")) { o["A_nature"]= in["A_nature"]; }
    out=std::move(o);
    return true;
}

// validate P_Alarms_PSM::C_Alarm_Category
static bool validate_P_Alarms_PSM__C_Alarm_Category(const json& in, json& out){
    if(!in.is_object()) return false;
    static const std::unordered_set<std::string> __keys = {"A_sourceID", "A_timeOfDataGeneration", "A_activeAlarmCount", "A_unacknowledgedAlarmCount", "A_categorisedActualAlarm_sourceID", "A_alarmCategorySpecification_sourceID"};
    if(!only_keys(in, __keys)) return false;
    json o=json::object();
    if(!in.contains("A_sourceID")) return false;
    if(in.contains("A_sourceID")) { const auto& obj=in["A_sourceID"]; json out_child; if(!validate_P_LDM_Common__T_IdentifierType(obj, out_child)) return false; o["A_sourceID"]=std::move(out_child); }
    if(!in.contains("A_timeOfDataGeneration")) return false;
    if(in.contains("A_timeOfDataGeneration")) { const auto& obj=in["A_timeOfDataGeneration"]; json out_child; if(!validate_P_LDM_Common__T_DateTimeType(obj, out_child)) return false; o["A_timeOfDataGeneration"]=std::move(out_child); }
    if(!in.contains("A_activeAlarmCount")) return false;
    if(in.contains("A_activeAlarmCount")) { o["A_activeAlarmCount"]= in["A_activeAlarmCount"]; }
    if(!in.contains("A_unacknowledgedAlarmCount")) return false;
    if(in.contains("A_unacknowledgedAlarmCount")) { o["A_unacknowledgedAlarmCount"]= in["A_unacknowledgedAlarmCount"]; }
    if(!in.contains("A_categorisedActualAlarm_sourceID")) return false;
    if(in.contains("A_categorisedActualAlarm_sourceID")) { const auto& arr=in["A_categorisedActualAlarm_sourceID"]; if(!arr.is_array()) return false; json a=json::array();
      for(const auto& el: arr){ json ch; if(!validate_P_LDM_Common__T_IdentifierType(el, ch)) return false; a.push_back(std::move(ch)); }
      o["A_categorisedActualAlarm_sourceID"]=std::move(a); }
    if(!in.contains("A_alarmCategorySpecification_sourceID")) return false;
    if(in.contains("A_alarmCategorySpecification_sourceID")) { const auto& obj=in["A_alarmCategorySpecification_sourceID"]; json out_child; if(!validate_P_LDM_Common__T_IdentifierType(obj, out_child)) return false; o["A_alarmCategorySpecification_sourceID"]=std::move(out_child); }
    out=std::move(o);
    return true;
}

// validate P_Alarms_PSM::C_Alarm_Category_Specification
static bool validate_P_Alarms_PSM__C_Alarm_Category_Specification(const json& in, json& out){
    if(!in.is_object()) return false;
    static const std::unordered_set<std::string> __keys = {"A_sourceID", "A_timeOfDataGeneration", "A_alarmCategoryName", "A_alarmCategoryAbbreviation", "A_isAutoAcknowledged", "A_automaticAcknowledgeTimeout", "A_hideOnAcknowledge", "A_isRepeated", "A_repeatTimeout", "A_categorisedConditionSpecification_sourceID", "A_notifyingToneSpecification_sourceID", "A_alarmCategory_sourceID"};
    if(!only_keys(in, __keys)) return false;
    json o=json::object();
    if(!in.contains("A_sourceID")) return false;
    if(in.contains("A_sourceID")) { const auto& obj=in["A_sourceID"]; json out_child; if(!validate_P_LDM_Common__T_IdentifierType(obj, out_child)) return false; o["A_sourceID"]=std::move(out_child); }
    if(!in.contains("A_timeOfDataGeneration")) return false;
    if(in.contains("A_timeOfDataGeneration")) { const auto& obj=in["A_timeOfDataGeneration"]; json out_child; if(!validate_P_LDM_Common__T_DateTimeType(obj, out_child)) return false; o["A_timeOfDataGeneration"]=std::move(out_child); }
    if(!in.contains("A_alarmCategoryName")) return false;
    if(in.contains("A_alarmCategoryName")) {
        const auto& v=in["A_alarmCategoryName"]; if(!v.is_string()) return false;
        std::string tok=v.get<std::string>();
        if(!kEnum_T_AlarmCategoryType.count(tok)) return false;
        o["A_alarmCategoryName"]=std::move(tok);
    }
    if(!in.contains("A_alarmCategoryAbbreviation")) return false;
    if(in.contains("A_alarmCategoryAbbreviation")) { o["A_alarmCategoryAbbreviation"]= in["A_alarmCategoryAbbreviation"]; }
    if(!in.contains("A_isAutoAcknowledged")) return false;
    if(in.contains("A_isAutoAcknowledged")) { o["A_isAutoAcknowledged"]= in["A_isAutoAcknowledged"]; }
    if(!in.contains("A_automaticAcknowledgeTimeout")) return false;
    if(in.contains("A_automaticAcknowledgeTimeout")) { const auto& obj=in["A_automaticAcknowledgeTimeout"]; json out_child; if(!validate_P_LDM_Common__T_DurationType(obj, out_child)) return false; o["A_automaticAcknowledgeTimeout"]=std::move(out_child); }
    if(!in.contains("A_hideOnAcknowledge")) return false;
    if(in.contains("A_hideOnAcknowledge")) { o["A_hideOnAcknowledge"]= in["A_hideOnAcknowledge"]; }
    if(!in.contains("A_isRepeated")) return false;
    if(in.contains("A_isRepeated")) { o["A_isRepeated"]= in["A_isRepeated"]; }
    if(!in.contains("A_repeatTimeout")) return false;
    if(in.contains("A_repeatTimeout")) { const auto& obj=in["A_repeatTimeout"]; json out_child; if(!validate_P_LDM_Common__T_DurationType(obj, out_child)) return false; o["A_repeatTimeout"]=std::move(out_child); }
    if(!in.contains("A_categorisedConditionSpecification_sourceID")) return false;
    if(in.contains("A_categorisedConditionSpecification_sourceID")) { const auto& arr=in["A_categorisedConditionSpecification_sourceID"]; if(!arr.is_array()) return false; json a=json::array();
      for(const auto& el: arr){ json ch; if(!validate_P_LDM_Common__T_IdentifierType(el, ch)) return false; a.push_back(std::move(ch)); }
      o["A_categorisedConditionSpecification_sourceID"]=std::move(a); }
    if(!in.contains("A_notifyingToneSpecification_sourceID")) return false;
    if(in.contains("A_notifyingToneSpecification_sourceID")) { const auto& obj=in["A_notifyingToneSpecification_sourceID"]; json out_child; if(!validate_P_LDM_Common__T_IdentifierType(obj, out_child)) return false; o["A_notifyingToneSpecification_sourceID"]=std::move(out_child); }
    if(!in.contains("A_alarmCategory_sourceID")) return false;
    if(in.contains("A_alarmCategory_sourceID")) { const auto& obj=in["A_alarmCategory_sourceID"]; json out_child; if(!validate_P_LDM_Common__T_IdentifierType(obj, out_child)) return false; o["A_alarmCategory_sourceID"]=std::move(out_child); }
    out=std::move(o);
    return true;
}

// validate P_Alarms_PSM::C_Alarm_Condition_Specification
static bool validate_P_Alarms_PSM__C_Alarm_Condition_Specification(const json& in, json& out){
    if(!in.is_object()) return false;
    static const std::unordered_set<std::string> __keys = {"A_sourceID", "A_timeOfDataGeneration", "A_subsystemName", "A_componentName", "A_measure", "A_nature", "A_alarmConditionCategory", "A_alarmConditionName", "A_hasMultipleInstances", "A_overrideState", "A_actualAlarmCondition_sourceID", "A_alarmCategory_sourceID", "A_interestedRole_sourceID"};
    if(!only_keys(in, __keys)) return false;
    json o=json::object();
    if(!in.contains("A_sourceID")) return false;
    if(in.contains("A_sourceID")) { const auto& obj=in["A_sourceID"]; json out_child; if(!validate_P_LDM_Common__T_IdentifierType(obj, out_child)) return false; o["A_sourceID"]=std::move(out_child); }
    if(!in.contains("A_timeOfDataGeneration")) return false;
    if(in.contains("A_timeOfDataGeneration")) { const auto& obj=in["A_timeOfDataGeneration"]; json out_child; if(!validate_P_LDM_Common__T_DateTimeType(obj, out_child)) return false; o["A_timeOfDataGeneration"]=std::move(out_child); }
    if(!in.contains("A_subsystemName")) return false;
    if(in.contains("A_subsystemName")) { o["A_subsystemName"]= in["A_subsystemName"]; }
    if(!in.contains("A_componentName")) return false;
    if(in.contains("A_componentName")) { o["A_componentName"]= in["A_componentName"]; }
    if(!in.contains("A_measure")) return false;
    if(in.contains("A_measure")) { o["A_measure"]= in["A_measure"]; }
    if(!in.contains("A_nature")) return false;
    if(in.contains("A_nature")) { o["A_nature"]= in["A_nature"]; }
    if(!in.contains("A_alarmConditionCategory")) return false;
    if(in.contains("A_alarmConditionCategory")) { o["A_alarmConditionCategory"]= in["A_alarmConditionCategory"]; }
    if(!in.contains("A_alarmConditionName")) return false;
    if(in.contains("A_alarmConditionName")) { o["A_alarmConditionName"]= in["A_alarmConditionName"]; }
    if(!in.contains("A_hasMultipleInstances")) return false;
    if(in.contains("A_hasMultipleInstances")) { o["A_hasMultipleInstances"]= in["A_hasMultipleInstances"]; }
    if(!in.contains("A_overrideState")) return false;
    if(in.contains("A_overrideState")) {
        const auto& v=in["A_overrideState"]; if(!v.is_string()) return false;
        std::string tok=v.get<std::string>();
        if(!kEnum_T_Alarm_Condition_Specification_StateType.count(tok)) return false;
        o["A_overrideState"]=std::move(tok);
    }
    if(!in.contains("A_actualAlarmCondition_sourceID")) return false;
    if(in.contains("A_actualAlarmCondition_sourceID")) { const auto& arr=in["A_actualAlarmCondition_sourceID"]; if(!arr.is_array()) return false; json a=json::array();
      for(const auto& el: arr){ json ch; if(!validate_P_LDM_Common__T_IdentifierType(el, ch)) return false; a.push_back(std::move(ch)); }
      o["A_actualAlarmCondition_sourceID"]=std::move(a); }
    if(!in.contains("A_alarmCategory_sourceID")) return false;
    if(in.contains("A_alarmCategory_sourceID")) { const auto& obj=in["A_alarmCategory_sourceID"]; json out_child; if(!validate_P_LDM_Common__T_IdentifierType(obj, out_child)) return false; o["A_alarmCategory_sourceID"]=std::move(out_child); }
    if(!in.contains("A_interestedRole_sourceID")) return false;
    if(in.contains("A_interestedRole_sourceID")) { const auto& arr=in["A_interestedRole_sourceID"]; if(!arr.is_array()) return false; json a=json::array();
      for(const auto& el: arr){ json ch; if(!validate_P_LDM_Common__T_IdentifierType(el, ch)) return false; a.push_back(std::move(ch)); }
      o["A_interestedRole_sourceID"]=std::move(a); }
    out=std::move(o);
    return true;
}

// validate P_Alarms_PSM::C_Alarm_Condition_Specification_isOfInterestToCrewRole
static bool validate_P_Alarms_PSM__C_Alarm_Condition_Specification_isOfInterestToCrewRole(const json& in, json& out){
    if(!in.is_object()) return false;
    static const std::unordered_set<std::string> __keys = {"A_recipientID", "A_sourceID", "A_referenceNum", "A_timeOfDataGeneration", "A_crewRole"};
    if(!only_keys(in, __keys)) return false;
    json o=json::object();
    if(!in.contains("A_recipientID")) return false;
    if(in.contains("A_recipientID")) { const auto& obj=in["A_recipientID"]; json out_child; if(!validate_P_LDM_Common__T_IdentifierType(obj, out_child)) return false; o["A_recipientID"]=std::move(out_child); }
    if(!in.contains("A_sourceID")) return false;
    if(in.contains("A_sourceID")) { const auto& obj=in["A_sourceID"]; json out_child; if(!validate_P_LDM_Common__T_IdentifierType(obj, out_child)) return false; o["A_sourceID"]=std::move(out_child); }
    if(!in.contains("A_referenceNum")) return false;
    if(in.contains("A_referenceNum")) { o["A_referenceNum"]= in["A_referenceNum"]; }
    if(!in.contains("A_timeOfDataGeneration")) return false;
    if(in.contains("A_timeOfDataGeneration")) { const auto& obj=in["A_timeOfDataGeneration"]; json out_child; if(!validate_P_LDM_Common__T_DateTimeType(obj, out_child)) return false; o["A_timeOfDataGeneration"]=std::move(out_child); }
    if(!in.contains("A_crewRole")) return false;
    if(in.contains("A_crewRole")) { o["A_crewRole"]= in["A_crewRole"]; }
    out=std::move(o);
    return true;
}

// validate P_Alarms_PSM::C_Alarm_Condition_Specification_raiseAlarmCondition
static bool validate_P_Alarms_PSM__C_Alarm_Condition_Specification_raiseAlarmCondition(const json& in, json& out){
    if(!in.is_object()) return false;
    static const std::unordered_set<std::string> __keys = {"A_recipientID", "A_sourceID", "A_referenceNum", "A_timeOfDataGeneration", "A_componentName", "A_subsystemName", "A_measure", "A_nature"};
    if(!only_keys(in, __keys)) return false;
    json o=json::object();
    if(!in.contains("A_recipientID")) return false;
    if(in.contains("A_recipientID")) { const auto& obj=in["A_recipientID"]; json out_child; if(!validate_P_LDM_Common__T_IdentifierType(obj, out_child)) return false; o["A_recipientID"]=std::move(out_child); }
    if(!in.contains("A_sourceID")) return false;
    if(in.contains("A_sourceID")) { const auto& obj=in["A_sourceID"]; json out_child; if(!validate_P_LDM_Common__T_IdentifierType(obj, out_child)) return false; o["A_sourceID"]=std::move(out_child); }
    if(!in.contains("A_referenceNum")) return false;
    if(in.contains("A_referenceNum")) { o["A_referenceNum"]= in["A_referenceNum"]; }
    if(!in.contains("A_timeOfDataGeneration")) return false;
    if(in.contains("A_timeOfDataGeneration")) { const auto& obj=in["A_timeOfDataGeneration"]; json out_child; if(!validate_P_LDM_Common__T_DateTimeType(obj, out_child)) return false; o["A_timeOfDataGeneration"]=std::move(out_child); }
    if(!in.contains("A_componentName")) return false;
    if(in.contains("A_componentName")) { o["A_componentName"]= in["A_componentName"]; }
    if(!in.contains("A_subsystemName")) return false;
    if(in.contains("A_subsystemName")) { o["A_subsystemName"]= in["A_subsystemName"]; }
    if(!in.contains("A_measure")) return false;
    if(in.contains("A_measure")) { o["A_measure"]= in["A_measure"]; }
    if(!in.contains("A_nature")) return false;
    if(in.contains("A_nature")) { o["A_nature"]= in["A_nature"]; }
    out=std::move(o);
    return true;
}

// validate P_Alarms_PSM::C_Crew_Role_In_Mission_State
static bool validate_P_Alarms_PSM__C_Crew_Role_In_Mission_State(const json& in, json& out){
    if(!in.is_object()) return false;
    static const std::unordered_set<std::string> __keys = {"A_sourceID", "A_timeOfDataGeneration", "A_crewRoleName", "A_relevantAlarmType_sourceID", "A_missionState_sourceID"};
    if(!only_keys(in, __keys)) return false;
    json o=json::object();
    if(!in.contains("A_sourceID")) return false;
    if(in.contains("A_sourceID")) { const auto& obj=in["A_sourceID"]; json out_child; if(!validate_P_LDM_Common__T_IdentifierType(obj, out_child)) return false; o["A_sourceID"]=std::move(out_child); }
    if(!in.contains("A_timeOfDataGeneration")) return false;
    if(in.contains("A_timeOfDataGeneration")) { const auto& obj=in["A_timeOfDataGeneration"]; json out_child; if(!validate_P_LDM_Common__T_DateTimeType(obj, out_child)) return false; o["A_timeOfDataGeneration"]=std::move(out_child); }
    if(!in.contains("A_crewRoleName")) return false;
    if(in.contains("A_crewRoleName")) { o["A_crewRoleName"]= in["A_crewRoleName"]; }
    if(!in.contains("A_relevantAlarmType_sourceID")) return false;
    if(in.contains("A_relevantAlarmType_sourceID")) { const auto& arr=in["A_relevantAlarmType_sourceID"]; if(!arr.is_array()) return false; json a=json::array();
      for(const auto& el: arr){ json ch; if(!validate_P_LDM_Common__T_IdentifierType(el, ch)) return false; a.push_back(std::move(ch)); }
      o["A_relevantAlarmType_sourceID"]=std::move(a); }
    if(!in.contains("A_missionState_sourceID")) return false;
    if(in.contains("A_missionState_sourceID")) { const auto& arr=in["A_missionState_sourceID"]; if(!arr.is_array()) return false; json a=json::array();
      for(const auto& el: arr){ json ch; if(!validate_P_LDM_Common__T_IdentifierType(el, ch)) return false; a.push_back(std::move(ch)); }
      o["A_missionState_sourceID"]=std::move(a); }
    out=std::move(o);
    return true;
}

// validate P_Alarms_PSM::C_Mission_State
static bool validate_P_Alarms_PSM__C_Mission_State(const json& in, json& out){
    if(!in.is_object()) return false;
    static const std::unordered_set<std::string> __keys = {"A_sourceID", "A_timeOfDataGeneration", "A_missionState", "A_missionStateName", "A_crewRoleInMissionState_sourceID", "A_ownPlatform_sourceID"};
    if(!only_keys(in, __keys)) return false;
    json o=json::object();
    if(!in.contains("A_sourceID")) return false;
    if(in.contains("A_sourceID")) { const auto& obj=in["A_sourceID"]; json out_child; if(!validate_P_LDM_Common__T_IdentifierType(obj, out_child)) return false; o["A_sourceID"]=std::move(out_child); }
    if(!in.contains("A_timeOfDataGeneration")) return false;
    if(in.contains("A_timeOfDataGeneration")) { const auto& obj=in["A_timeOfDataGeneration"]; json out_child; if(!validate_P_LDM_Common__T_DateTimeType(obj, out_child)) return false; o["A_timeOfDataGeneration"]=std::move(out_child); }
    if(!in.contains("A_missionState")) return false;
    if(in.contains("A_missionState")) {
        const auto& v=in["A_missionState"]; if(!v.is_string()) return false;
        std::string tok=v.get<std::string>();
        if(!kEnum_T_MissionStateType.count(tok)) return false;
        o["A_missionState"]=std::move(tok);
    }
    if(!in.contains("A_missionStateName")) return false;
    if(in.contains("A_missionStateName")) { o["A_missionStateName"]= in["A_missionStateName"]; }
    if(!in.contains("A_crewRoleInMissionState_sourceID")) return false;
    if(in.contains("A_crewRoleInMissionState_sourceID")) { const auto& arr=in["A_crewRoleInMissionState_sourceID"]; if(!arr.is_array()) return false; json a=json::array();
      for(const auto& el: arr){ json ch; if(!validate_P_LDM_Common__T_IdentifierType(el, ch)) return false; a.push_back(std::move(ch)); }
      o["A_crewRoleInMissionState_sourceID"]=std::move(a); }
    if(!in.contains("A_ownPlatform_sourceID")) return false;
    if(in.contains("A_ownPlatform_sourceID")) { const auto& obj=in["A_ownPlatform_sourceID"]; json out_child; if(!validate_P_LDM_Common__T_IdentifierType(obj, out_child)) return false; o["A_ownPlatform_sourceID"]=std::move(out_child); }
    out=std::move(o);
    return true;
}

// validate P_Alarms_PSM::C_Mission_State_setMissionState
static bool validate_P_Alarms_PSM__C_Mission_State_setMissionState(const json& in, json& out){
    if(!in.is_object()) return false;
    static const std::unordered_set<std::string> __keys = {"A_recipientID", "A_sourceID", "A_referenceNum", "A_timeOfDataGeneration", "A_missionState", "A_missionStateName"};
    if(!only_keys(in, __keys)) return false;
    json o=json::object();
    if(!in.contains("A_recipientID")) return false;
    if(in.contains("A_recipientID")) { const auto& obj=in["A_recipientID"]; json out_child; if(!validate_P_LDM_Common__T_IdentifierType(obj, out_child)) return false; o["A_recipientID"]=std::move(out_child); }
    if(!in.contains("A_sourceID")) return false;
    if(in.contains("A_sourceID")) { const auto& obj=in["A_sourceID"]; json out_child; if(!validate_P_LDM_Common__T_IdentifierType(obj, out_child)) return false; o["A_sourceID"]=std::move(out_child); }
    if(!in.contains("A_referenceNum")) return false;
    if(in.contains("A_referenceNum")) { o["A_referenceNum"]= in["A_referenceNum"]; }
    if(!in.contains("A_timeOfDataGeneration")) return false;
    if(in.contains("A_timeOfDataGeneration")) { const auto& obj=in["A_timeOfDataGeneration"]; json out_child; if(!validate_P_LDM_Common__T_DateTimeType(obj, out_child)) return false; o["A_timeOfDataGeneration"]=std::move(out_child); }
    if(!in.contains("A_missionState")) return false;
    if(in.contains("A_missionState")) {
        const auto& v=in["A_missionState"]; if(!v.is_string()) return false;
        std::string tok=v.get<std::string>();
        if(!kEnum_T_MissionStateType.count(tok)) return false;
        o["A_missionState"]=std::move(tok);
    }
    if(!in.contains("A_missionStateName")) return false;
    if(in.contains("A_missionStateName")) { o["A_missionStateName"]= in["A_missionStateName"]; }
    out=std::move(o);
    return true;
}

// validate P_Alarms_PSM::C_Own_Platform
static bool validate_P_Alarms_PSM__C_Own_Platform(const json& in, json& out){
    if(!in.is_object()) return false;
    static const std::unordered_set<std::string> __keys = {"A_sourceID", "A_timeOfDataGeneration", "A_activeAlarmsExist", "A_possibleMissionState_sourceID"};
    if(!only_keys(in, __keys)) return false;
    json o=json::object();
    if(!in.contains("A_sourceID")) return false;
    if(in.contains("A_sourceID")) { const auto& obj=in["A_sourceID"]; json out_child; if(!validate_P_LDM_Common__T_IdentifierType(obj, out_child)) return false; o["A_sourceID"]=std::move(out_child); }
    if(!in.contains("A_timeOfDataGeneration")) return false;
    if(in.contains("A_timeOfDataGeneration")) { const auto& obj=in["A_timeOfDataGeneration"]; json out_child; if(!validate_P_LDM_Common__T_DateTimeType(obj, out_child)) return false; o["A_timeOfDataGeneration"]=std::move(out_child); }
    if(!in.contains("A_activeAlarmsExist")) return false;
    if(in.contains("A_activeAlarmsExist")) { o["A_activeAlarmsExist"]= in["A_activeAlarmsExist"]; }
    if(!in.contains("A_possibleMissionState_sourceID")) return false;
    if(in.contains("A_possibleMissionState_sourceID")) { const auto& arr=in["A_possibleMissionState_sourceID"]; if(!arr.is_array()) return false; json a=json::array();
      for(const auto& el: arr){ json ch; if(!validate_P_LDM_Common__T_IdentifierType(el, ch)) return false; a.push_back(std::move(ch)); }
      o["A_possibleMissionState_sourceID"]=std::move(a); }
    out=std::move(o);
    return true;
}

// validate P_Alarms_PSM::C_Tone_Specification
static bool validate_P_Alarms_PSM__C_Tone_Specification(const json& in, json& out){
    if(!in.is_object()) return false;
    static const std::unordered_set<std::string> __keys = {"A_sourceID", "A_timeOfDataGeneration", "A_toneFrequency", "A_toneModulationType", "A_toneRepetitionFrequency", "A_toneMaxVolume", "A_alarmCategorySpecification_sourceID"};
    if(!only_keys(in, __keys)) return false;
    json o=json::object();
    if(!in.contains("A_sourceID")) return false;
    if(in.contains("A_sourceID")) { const auto& obj=in["A_sourceID"]; json out_child; if(!validate_P_LDM_Common__T_IdentifierType(obj, out_child)) return false; o["A_sourceID"]=std::move(out_child); }
    if(!in.contains("A_timeOfDataGeneration")) return false;
    if(in.contains("A_timeOfDataGeneration")) { const auto& obj=in["A_timeOfDataGeneration"]; json out_child; if(!validate_P_LDM_Common__T_DateTimeType(obj, out_child)) return false; o["A_timeOfDataGeneration"]=std::move(out_child); }
    if(!in.contains("A_toneFrequency")) return false;
    if(in.contains("A_toneFrequency")) { o["A_toneFrequency"]= in["A_toneFrequency"]; }
    if(!in.contains("A_toneModulationType")) return false;
    if(in.contains("A_toneModulationType")) { o["A_toneModulationType"]= in["A_toneModulationType"]; }
    if(!in.contains("A_toneRepetitionFrequency")) return false;
    if(in.contains("A_toneRepetitionFrequency")) { o["A_toneRepetitionFrequency"]= in["A_toneRepetitionFrequency"]; }
    if(!in.contains("A_toneMaxVolume")) return false;
    if(in.contains("A_toneMaxVolume")) { o["A_toneMaxVolume"]= in["A_toneMaxVolume"]; }
    if(!in.contains("A_alarmCategorySpecification_sourceID")) return false;
    if(in.contains("A_alarmCategorySpecification_sourceID")) { const auto& obj=in["A_alarmCategorySpecification_sourceID"]; json out_child; if(!validate_P_LDM_Common__T_IdentifierType(obj, out_child)) return false; o["A_alarmCategorySpecification_sourceID"]=std::move(out_child); }
    out=std::move(o);
    return true;
}

// validate P_LDM_Common::T_AngularAcceleration3DType
static bool validate_P_LDM_Common__T_AngularAcceleration3DType(const json& in, json& out){
    if(!in.is_object()) return false;
    static const std::unordered_set<std::string> __keys = {"A_pitch", "A_roll", "A_yaw"};
    if(!only_keys(in, __keys)) return false;
    json o=json::object();
    if(!in.contains("A_pitch")) return false;
    if(in.contains("A_pitch")) { const auto& v=in["A_pitch"]; if(!v.is_number()) return false; o["A_pitch"]=v; }
    if(!in.contains("A_roll")) return false;
    if(in.contains("A_roll")) { const auto& v=in["A_roll"]; if(!v.is_number()) return false; o["A_roll"]=v; }
    if(!in.contains("A_yaw")) return false;
    if(in.contains("A_yaw")) { const auto& v=in["A_yaw"]; if(!v.is_number()) return false; o["A_yaw"]=v; }
    out=std::move(o);
    return true;
}

// validate P_LDM_Common::T_AngularVelocity3DType
static bool validate_P_LDM_Common__T_AngularVelocity3DType(const json& in, json& out){
    if(!in.is_object()) return false;
    static const std::unordered_set<std::string> __keys = {"A_pitch", "A_roll", "A_yaw"};
    if(!only_keys(in, __keys)) return false;
    json o=json::object();
    if(!in.contains("A_pitch")) return false;
    if(in.contains("A_pitch")) { const auto& v=in["A_pitch"]; if(!v.is_number()) return false; o["A_pitch"]=v; }
    if(!in.contains("A_roll")) return false;
    if(in.contains("A_roll")) { const auto& v=in["A_roll"]; if(!v.is_number()) return false; o["A_roll"]=v; }
    if(!in.contains("A_yaw")) return false;
    if(in.contains("A_yaw")) { const auto& v=in["A_yaw"]; if(!v.is_number()) return false; o["A_yaw"]=v; }
    out=std::move(o);
    return true;
}

// validate P_LDM_Common::T_AttitudeType
static bool validate_P_LDM_Common__T_AttitudeType(const json& in, json& out){
    if(!in.is_object()) return false;
    static const std::unordered_set<std::string> __keys = {"A_pitch", "A_roll", "A_yaw"};
    if(!only_keys(in, __keys)) return false;
    json o=json::object();
    if(!in.contains("A_pitch")) return false;
    if(in.contains("A_pitch")) { const auto& v=in["A_pitch"]; if(!v.is_number()) return false; o["A_pitch"]=v; }
    if(!in.contains("A_roll")) return false;
    if(in.contains("A_roll")) { const auto& v=in["A_roll"]; if(!v.is_number()) return false; o["A_roll"]=v; }
    if(!in.contains("A_yaw")) return false;
    if(in.contains("A_yaw")) { const auto& v=in["A_yaw"]; if(!v.is_number()) return false; o["A_yaw"]=v; }
    out=std::move(o);
    return true;
}

// validate P_LDM_Common::T_Coordinate2DType
static bool validate_P_LDM_Common__T_Coordinate2DType(const json& in, json& out){
    if(!in.is_object()) return false;
    static const std::unordered_set<std::string> __keys = {"A_latitude", "A_longitude"};
    if(!only_keys(in, __keys)) return false;
    json o=json::object();
    if(!in.contains("A_latitude")) return false;
    if(in.contains("A_latitude")) { const auto& v=in["A_latitude"]; if(!v.is_number()) return false; o["A_latitude"]=v; }
    if(!in.contains("A_longitude")) return false;
    if(in.contains("A_longitude")) { const auto& v=in["A_longitude"]; if(!v.is_number()) return false; o["A_longitude"]=v; }
    out=std::move(o);
    return true;
}

// validate P_LDM_Common::T_Coordinate3DType
static bool validate_P_LDM_Common__T_Coordinate3DType(const json& in, json& out){
    if(!in.is_object()) return false;
    static const std::unordered_set<std::string> __keys = {"A_altitude", "A_latitude", "A_longitude"};
    if(!only_keys(in, __keys)) return false;
    json o=json::object();
    if(!in.contains("A_altitude")) return false;
    if(in.contains("A_altitude")) { const auto& v=in["A_altitude"]; if(!v.is_number()) return false; o["A_altitude"]=v; }
    if(!in.contains("A_latitude")) return false;
    if(in.contains("A_latitude")) { const auto& v=in["A_latitude"]; if(!v.is_number()) return false; o["A_latitude"]=v; }
    if(!in.contains("A_longitude")) return false;
    if(in.contains("A_longitude")) { const auto& v=in["A_longitude"]; if(!v.is_number()) return false; o["A_longitude"]=v; }
    out=std::move(o);
    return true;
}

// validate P_LDM_Common::T_CoordinatePolar2DType
static bool validate_P_LDM_Common__T_CoordinatePolar2DType(const json& in, json& out){
    if(!in.is_object()) return false;
    static const std::unordered_set<std::string> __keys = {"A_angle", "A_range"};
    if(!only_keys(in, __keys)) return false;
    json o=json::object();
    if(!in.contains("A_angle")) return false;
    if(in.contains("A_angle")) { const auto& v=in["A_angle"]; if(!v.is_number()) return false; o["A_angle"]=v; }
    if(!in.contains("A_range")) return false;
    if(in.contains("A_range")) { const auto& v=in["A_range"]; if(!v.is_number()) return false; o["A_range"]=v; }
    out=std::move(o);
    return true;
}

// validate P_LDM_Common::T_CoordinatePolar3DType
static bool validate_P_LDM_Common__T_CoordinatePolar3DType(const json& in, json& out){
    if(!in.is_object()) return false;
    static const std::unordered_set<std::string> __keys = {"A_angle", "A_elevation", "A_range"};
    if(!only_keys(in, __keys)) return false;
    json o=json::object();
    if(!in.contains("A_angle")) return false;
    if(in.contains("A_angle")) { const auto& v=in["A_angle"]; if(!v.is_number()) return false; o["A_angle"]=v; }
    if(!in.contains("A_elevation")) return false;
    if(in.contains("A_elevation")) { const auto& v=in["A_elevation"]; if(!v.is_number()) return false; o["A_elevation"]=v; }
    if(!in.contains("A_range")) return false;
    if(in.contains("A_range")) { const auto& v=in["A_range"]; if(!v.is_number()) return false; o["A_range"]=v; }
    out=std::move(o);
    return true;
}

// validate P_LDM_Common::T_DateTimeType
static bool validate_P_LDM_Common__T_DateTimeType(const json& in, json& out){
    if(!in.is_object()) return false;
    static const std::unordered_set<std::string> __keys = {"A_second", "A_nanoseconds"};
    if(!only_keys(in, __keys)) return false;
    json o=json::object();
    if(!in.contains("A_second")) return false;
    if(in.contains("A_second")) { const auto& v=in["A_second"]; if(!v.is_number_integer()) return false; o["A_second"]=v; }
    if(!in.contains("A_nanoseconds")) return false;
    if(in.contains("A_nanoseconds")) { const auto& v=in["A_nanoseconds"]; if(!v.is_number_integer()) return false; o["A_nanoseconds"]=v; }
    out=std::move(o);
    return true;
}

// validate P_LDM_Common::T_DurationType
static bool validate_P_LDM_Common__T_DurationType(const json& in, json& out){
    if(!in.is_object()) return false;
    static const std::unordered_set<std::string> __keys = {"A_seconds", "A_nanoseconds"};
    if(!only_keys(in, __keys)) return false;
    json o=json::object();
    if(!in.contains("A_seconds")) return false;
    if(in.contains("A_seconds")) { const auto& v=in["A_seconds"]; if(!v.is_number_integer()) return false; o["A_seconds"]=v; }
    if(!in.contains("A_nanoseconds")) return false;
    if(in.contains("A_nanoseconds")) { const auto& v=in["A_nanoseconds"]; if(!v.is_number_integer()) return false; o["A_nanoseconds"]=v; }
    out=std::move(o);
    return true;
}

// validate P_LDM_Common::T_IdentifierType
static bool validate_P_LDM_Common__T_IdentifierType(const json& in, json& out){
    if(!in.is_object()) return false;
    static const std::unordered_set<std::string> __keys = {"A_resourceId", "A_instanceId"};
    if(!only_keys(in, __keys)) return false;
    json o=json::object();
    if(!in.contains("A_resourceId")) return false;
    if(in.contains("A_resourceId")) { const auto& v=in["A_resourceId"]; if(!v.is_number_integer()) return false; o["A_resourceId"]=v; }
    if(!in.contains("A_instanceId")) return false;
    if(in.contains("A_instanceId")) { const auto& v=in["A_instanceId"]; if(!v.is_number_integer()) return false; o["A_instanceId"]=v; }
    out=std::move(o);
    return true;
}

// validate P_LDM_Common::T_LinearAcceleration3DType
static bool validate_P_LDM_Common__T_LinearAcceleration3DType(const json& in, json& out){
    if(!in.is_object()) return false;
    static const std::unordered_set<std::string> __keys = {"A_xAcceleration", "A_yAcceleration", "A_zAcceleration"};
    if(!only_keys(in, __keys)) return false;
    json o=json::object();
    if(!in.contains("A_xAcceleration")) return false;
    if(in.contains("A_xAcceleration")) { const auto& v=in["A_xAcceleration"]; if(!v.is_number()) return false; o["A_xAcceleration"]=v; }
    if(!in.contains("A_yAcceleration")) return false;
    if(in.contains("A_yAcceleration")) { const auto& v=in["A_yAcceleration"]; if(!v.is_number()) return false; o["A_yAcceleration"]=v; }
    if(!in.contains("A_zAcceleration")) return false;
    if(in.contains("A_zAcceleration")) { const auto& v=in["A_zAcceleration"]; if(!v.is_number()) return false; o["A_zAcceleration"]=v; }
    out=std::move(o);
    return true;
}

// validate P_LDM_Common::T_LinearOffsetType
static bool validate_P_LDM_Common__T_LinearOffsetType(const json& in, json& out){
    if(!in.is_object()) return false;
    static const std::unordered_set<std::string> __keys = {"A_xOffset", "A_yOffset", "A_zOffset"};
    if(!only_keys(in, __keys)) return false;
    json o=json::object();
    if(!in.contains("A_xOffset")) return false;
    if(in.contains("A_xOffset")) { const auto& v=in["A_xOffset"]; if(!v.is_number()) return false; o["A_xOffset"]=v; }
    if(!in.contains("A_yOffset")) return false;
    if(in.contains("A_yOffset")) { const auto& v=in["A_yOffset"]; if(!v.is_number()) return false; o["A_yOffset"]=v; }
    if(!in.contains("A_zOffset")) return false;
    if(in.contains("A_zOffset")) { const auto& v=in["A_zOffset"]; if(!v.is_number()) return false; o["A_zOffset"]=v; }
    out=std::move(o);
    return true;
}

// validate P_LDM_Common::T_LinearSpeed3DType
static bool validate_P_LDM_Common__T_LinearSpeed3DType(const json& in, json& out){
    if(!in.is_object()) return false;
    static const std::unordered_set<std::string> __keys = {"A_xSpeed", "A_ySpeed", "A_zSpeed"};
    if(!only_keys(in, __keys)) return false;
    json o=json::object();
    if(!in.contains("A_xSpeed")) return false;
    if(in.contains("A_xSpeed")) { const auto& v=in["A_xSpeed"]; if(!v.is_number()) return false; o["A_xSpeed"]=v; }
    if(!in.contains("A_ySpeed")) return false;
    if(in.contains("A_ySpeed")) { const auto& v=in["A_ySpeed"]; if(!v.is_number()) return false; o["A_ySpeed"]=v; }
    if(!in.contains("A_zSpeed")) return false;
    if(in.contains("A_zSpeed")) { const auto& v=in["A_zSpeed"]; if(!v.is_number()) return false; o["A_zSpeed"]=v; }
    out=std::move(o);
    return true;
}

// validate P_LDM_Common::T_LinearVelocity2DType
static bool validate_P_LDM_Common__T_LinearVelocity2DType(const json& in, json& out){
    if(!in.is_object()) return false;
    static const std::unordered_set<std::string> __keys = {"A_heading", "A_speed"};
    if(!only_keys(in, __keys)) return false;
    json o=json::object();
    if(!in.contains("A_heading")) return false;
    if(in.contains("A_heading")) { const auto& v=in["A_heading"]; if(!v.is_number()) return false; o["A_heading"]=v; }
    if(!in.contains("A_speed")) return false;
    if(in.contains("A_speed")) { const auto& v=in["A_speed"]; if(!v.is_number()) return false; o["A_speed"]=v; }
    out=std::move(o);
    return true;
}

// validate P_LDM_Common::T_LinearVelocity3DType
static bool validate_P_LDM_Common__T_LinearVelocity3DType(const json& in, json& out){
    if(!in.is_object()) return false;
    static const std::unordered_set<std::string> __keys = {"A_heading", "A_speed", "A_vrate"};
    if(!only_keys(in, __keys)) return false;
    json o=json::object();
    if(!in.contains("A_heading")) return false;
    if(in.contains("A_heading")) { const auto& v=in["A_heading"]; if(!v.is_number()) return false; o["A_heading"]=v; }
    if(!in.contains("A_speed")) return false;
    if(in.contains("A_speed")) { const auto& v=in["A_speed"]; if(!v.is_number()) return false; o["A_speed"]=v; }
    if(!in.contains("A_vrate")) return false;
    if(in.contains("A_vrate")) { const auto& v=in["A_vrate"]; if(!v.is_number()) return false; o["A_vrate"]=v; }
    out=std::move(o);
    return true;
}

// validate P_LDM_Common::T_PointPolar3DType
static bool validate_P_LDM_Common__T_PointPolar3DType(const json& in, json& out){
    if(!in.is_object()) return false;
    static const std::unordered_set<std::string> __keys = {"A_angle", "A_elevation", "A_radius"};
    if(!only_keys(in, __keys)) return false;
    json o=json::object();
    if(!in.contains("A_angle")) return false;
    if(in.contains("A_angle")) { const auto& v=in["A_angle"]; if(!v.is_number()) return false; o["A_angle"]=v; }
    if(!in.contains("A_elevation")) return false;
    if(in.contains("A_elevation")) { const auto& v=in["A_elevation"]; if(!v.is_number()) return false; o["A_elevation"]=v; }
    if(!in.contains("A_radius")) return false;
    if(in.contains("A_radius")) { const auto& v=in["A_radius"]; if(!v.is_number()) return false; o["A_radius"]=v; }
    out=std::move(o);
    return true;
}

// validate P_LDM_Common::T_Position2DType
static bool validate_P_LDM_Common__T_Position2DType(const json& in, json& out){
    if(!in.is_object()) return false;
    static const std::unordered_set<std::string> __keys = {"A_xPosition", "A_yPosition"};
    if(!only_keys(in, __keys)) return false;
    json o=json::object();
    if(!in.contains("A_xPosition")) return false;
    if(in.contains("A_xPosition")) { const auto& v=in["A_xPosition"]; if(!v.is_number_integer()) return false; o["A_xPosition"]=v; }
    if(!in.contains("A_yPosition")) return false;
    if(in.contains("A_yPosition")) { const auto& v=in["A_yPosition"]; if(!v.is_number_integer()) return false; o["A_yPosition"]=v; }
    out=std::move(o);
    return true;
}

// validate P_LDM_Common::T_RotationalOffsetType
static bool validate_P_LDM_Common__T_RotationalOffsetType(const json& in, json& out){
    if(!in.is_object()) return false;
    static const std::unordered_set<std::string> __keys = {"A_pitchOffset", "A_rollOffset", "A_yawOffset"};
    if(!only_keys(in, __keys)) return false;
    json o=json::object();
    if(!in.contains("A_pitchOffset")) return false;
    if(in.contains("A_pitchOffset")) { const auto& v=in["A_pitchOffset"]; if(!v.is_number()) return false; o["A_pitchOffset"]=v; }
    if(!in.contains("A_rollOffset")) return false;
    if(in.contains("A_rollOffset")) { const auto& v=in["A_rollOffset"]; if(!v.is_number()) return false; o["A_rollOffset"]=v; }
    if(!in.contains("A_yawOffset")) return false;
    if(in.contains("A_yawOffset")) { const auto& v=in["A_yawOffset"]; if(!v.is_number()) return false; o["A_yawOffset"]=v; }
    out=std::move(o);
    return true;
}

// validate P_LDM_Common::T_Size2DType
static bool validate_P_LDM_Common__T_Size2DType(const json& in, json& out){
    if(!in.is_object()) return false;
    static const std::unordered_set<std::string> __keys = {"A_xSize", "A_ySize"};
    if(!only_keys(in, __keys)) return false;
    json o=json::object();
    if(!in.contains("A_xSize")) return false;
    if(in.contains("A_xSize")) { const auto& v=in["A_xSize"]; if(!v.is_number_integer()) return false; o["A_xSize"]=v; }
    if(!in.contains("A_ySize")) return false;
    if(in.contains("A_ySize")) { const auto& v=in["A_ySize"]; if(!v.is_number_integer()) return false; o["A_ySize"]=v; }
    out=std::move(o);
    return true;
}

// validate StringMsg
static bool validate_StringMsg(const json& in, json& out){
    if(!in.is_object()) return false;
    static const std::unordered_set<std::string> __keys = {"text"};
    if(!only_keys(in, __keys)) return false;
    json o=json::object();
    if(!in.contains("text")) return false;
    if(in.contains("text")) { const auto& v=in["text"]; if(!v.is_string()) return false; std::string s=v.get<std::string>(); o["text"]=std::move(s); }
    out=std::move(o);
    return true;
}

// validate T_AngularAcceleration3DType
static bool validate_T_AngularAcceleration3DType(const json& in, json& out){
    if(!in.is_object()) return false;
    static const std::unordered_set<std::string> __keys = {"A_pitch", "A_roll", "A_yaw"};
    if(!only_keys(in, __keys)) return false;
    json o=json::object();
    if(!in.contains("A_pitch")) return false;
    if(in.contains("A_pitch")) { const auto& v=in["A_pitch"]; if(!v.is_number()) return false; o["A_pitch"]=v; }
    if(!in.contains("A_roll")) return false;
    if(in.contains("A_roll")) { const auto& v=in["A_roll"]; if(!v.is_number()) return false; o["A_roll"]=v; }
    if(!in.contains("A_yaw")) return false;
    if(in.contains("A_yaw")) { const auto& v=in["A_yaw"]; if(!v.is_number()) return false; o["A_yaw"]=v; }
    out=std::move(o);
    return true;
}

// validate T_AngularVelocity3DType
static bool validate_T_AngularVelocity3DType(const json& in, json& out){
    if(!in.is_object()) return false;
    static const std::unordered_set<std::string> __keys = {"A_pitch", "A_roll", "A_yaw"};
    if(!only_keys(in, __keys)) return false;
    json o=json::object();
    if(!in.contains("A_pitch")) return false;
    if(in.contains("A_pitch")) { const auto& v=in["A_pitch"]; if(!v.is_number()) return false; o["A_pitch"]=v; }
    if(!in.contains("A_roll")) return false;
    if(in.contains("A_roll")) { const auto& v=in["A_roll"]; if(!v.is_number()) return false; o["A_roll"]=v; }
    if(!in.contains("A_yaw")) return false;
    if(in.contains("A_yaw")) { const auto& v=in["A_yaw"]; if(!v.is_number()) return false; o["A_yaw"]=v; }
    out=std::move(o);
    return true;
}

// validate T_AttitudeType
static bool validate_T_AttitudeType(const json& in, json& out){
    if(!in.is_object()) return false;
    static const std::unordered_set<std::string> __keys = {"A_pitch", "A_roll", "A_yaw"};
    if(!only_keys(in, __keys)) return false;
    json o=json::object();
    if(!in.contains("A_pitch")) return false;
    if(in.contains("A_pitch")) { const auto& v=in["A_pitch"]; if(!v.is_number()) return false; o["A_pitch"]=v; }
    if(!in.contains("A_roll")) return false;
    if(in.contains("A_roll")) { const auto& v=in["A_roll"]; if(!v.is_number()) return false; o["A_roll"]=v; }
    if(!in.contains("A_yaw")) return false;
    if(in.contains("A_yaw")) { const auto& v=in["A_yaw"]; if(!v.is_number()) return false; o["A_yaw"]=v; }
    out=std::move(o);
    return true;
}

// validate T_Coordinate2DType
static bool validate_T_Coordinate2DType(const json& in, json& out){
    if(!in.is_object()) return false;
    static const std::unordered_set<std::string> __keys = {"A_latitude", "A_longitude"};
    if(!only_keys(in, __keys)) return false;
    json o=json::object();
    if(!in.contains("A_latitude")) return false;
    if(in.contains("A_latitude")) { const auto& v=in["A_latitude"]; if(!v.is_number()) return false; o["A_latitude"]=v; }
    if(!in.contains("A_longitude")) return false;
    if(in.contains("A_longitude")) { const auto& v=in["A_longitude"]; if(!v.is_number()) return false; o["A_longitude"]=v; }
    out=std::move(o);
    return true;
}

// validate T_Coordinate3DType
static bool validate_T_Coordinate3DType(const json& in, json& out){
    if(!in.is_object()) return false;
    static const std::unordered_set<std::string> __keys = {"A_altitude", "A_latitude", "A_longitude"};
    if(!only_keys(in, __keys)) return false;
    json o=json::object();
    if(!in.contains("A_altitude")) return false;
    if(in.contains("A_altitude")) { const auto& v=in["A_altitude"]; if(!v.is_number()) return false; o["A_altitude"]=v; }
    if(!in.contains("A_latitude")) return false;
    if(in.contains("A_latitude")) { const auto& v=in["A_latitude"]; if(!v.is_number()) return false; o["A_latitude"]=v; }
    if(!in.contains("A_longitude")) return false;
    if(in.contains("A_longitude")) { const auto& v=in["A_longitude"]; if(!v.is_number()) return false; o["A_longitude"]=v; }
    out=std::move(o);
    return true;
}

// validate T_CoordinatePolar2DType
static bool validate_T_CoordinatePolar2DType(const json& in, json& out){
    if(!in.is_object()) return false;
    static const std::unordered_set<std::string> __keys = {"A_angle", "A_range"};
    if(!only_keys(in, __keys)) return false;
    json o=json::object();
    if(!in.contains("A_angle")) return false;
    if(in.contains("A_angle")) { const auto& v=in["A_angle"]; if(!v.is_number()) return false; o["A_angle"]=v; }
    if(!in.contains("A_range")) return false;
    if(in.contains("A_range")) { const auto& v=in["A_range"]; if(!v.is_number()) return false; o["A_range"]=v; }
    out=std::move(o);
    return true;
}

// validate T_CoordinatePolar3DType
static bool validate_T_CoordinatePolar3DType(const json& in, json& out){
    if(!in.is_object()) return false;
    static const std::unordered_set<std::string> __keys = {"A_angle", "A_elevation", "A_range"};
    if(!only_keys(in, __keys)) return false;
    json o=json::object();
    if(!in.contains("A_angle")) return false;
    if(in.contains("A_angle")) { const auto& v=in["A_angle"]; if(!v.is_number()) return false; o["A_angle"]=v; }
    if(!in.contains("A_elevation")) return false;
    if(in.contains("A_elevation")) { const auto& v=in["A_elevation"]; if(!v.is_number()) return false; o["A_elevation"]=v; }
    if(!in.contains("A_range")) return false;
    if(in.contains("A_range")) { const auto& v=in["A_range"]; if(!v.is_number()) return false; o["A_range"]=v; }
    out=std::move(o);
    return true;
}

// validate T_DateTimeType
static bool validate_T_DateTimeType(const json& in, json& out){
    if(!in.is_object()) return false;
    static const std::unordered_set<std::string> __keys = {"A_second", "A_nanoseconds"};
    if(!only_keys(in, __keys)) return false;
    json o=json::object();
    if(!in.contains("A_second")) return false;
    if(in.contains("A_second")) { const auto& v=in["A_second"]; if(!v.is_number_integer()) return false; o["A_second"]=v; }
    if(!in.contains("A_nanoseconds")) return false;
    if(in.contains("A_nanoseconds")) { const auto& v=in["A_nanoseconds"]; if(!v.is_number_integer()) return false; o["A_nanoseconds"]=v; }
    out=std::move(o);
    return true;
}

// validate T_DurationType
static bool validate_T_DurationType(const json& in, json& out){
    if(!in.is_object()) return false;
    static const std::unordered_set<std::string> __keys = {"A_seconds", "A_nanoseconds"};
    if(!only_keys(in, __keys)) return false;
    json o=json::object();
    if(!in.contains("A_seconds")) return false;
    if(in.contains("A_seconds")) { const auto& v=in["A_seconds"]; if(!v.is_number_integer()) return false; o["A_seconds"]=v; }
    if(!in.contains("A_nanoseconds")) return false;
    if(in.contains("A_nanoseconds")) { const auto& v=in["A_nanoseconds"]; if(!v.is_number_integer()) return false; o["A_nanoseconds"]=v; }
    out=std::move(o);
    return true;
}

// validate T_IdentifierType
static bool validate_T_IdentifierType(const json& in, json& out){
    if(!in.is_object()) return false;
    static const std::unordered_set<std::string> __keys = {"A_resourceId", "A_instanceId"};
    if(!only_keys(in, __keys)) return false;
    json o=json::object();
    if(!in.contains("A_resourceId")) return false;
    if(in.contains("A_resourceId")) { const auto& v=in["A_resourceId"]; if(!v.is_number_integer()) return false; o["A_resourceId"]=v; }
    if(!in.contains("A_instanceId")) return false;
    if(in.contains("A_instanceId")) { const auto& v=in["A_instanceId"]; if(!v.is_number_integer()) return false; o["A_instanceId"]=v; }
    out=std::move(o);
    return true;
}

// validate T_LinearAcceleration3DType
static bool validate_T_LinearAcceleration3DType(const json& in, json& out){
    if(!in.is_object()) return false;
    static const std::unordered_set<std::string> __keys = {"A_xAcceleration", "A_yAcceleration", "A_zAcceleration"};
    if(!only_keys(in, __keys)) return false;
    json o=json::object();
    if(!in.contains("A_xAcceleration")) return false;
    if(in.contains("A_xAcceleration")) { const auto& v=in["A_xAcceleration"]; if(!v.is_number()) return false; o["A_xAcceleration"]=v; }
    if(!in.contains("A_yAcceleration")) return false;
    if(in.contains("A_yAcceleration")) { const auto& v=in["A_yAcceleration"]; if(!v.is_number()) return false; o["A_yAcceleration"]=v; }
    if(!in.contains("A_zAcceleration")) return false;
    if(in.contains("A_zAcceleration")) { const auto& v=in["A_zAcceleration"]; if(!v.is_number()) return false; o["A_zAcceleration"]=v; }
    out=std::move(o);
    return true;
}

// validate T_LinearOffsetType
static bool validate_T_LinearOffsetType(const json& in, json& out){
    if(!in.is_object()) return false;
    static const std::unordered_set<std::string> __keys = {"A_xOffset", "A_yOffset", "A_zOffset"};
    if(!only_keys(in, __keys)) return false;
    json o=json::object();
    if(!in.contains("A_xOffset")) return false;
    if(in.contains("A_xOffset")) { const auto& v=in["A_xOffset"]; if(!v.is_number()) return false; o["A_xOffset"]=v; }
    if(!in.contains("A_yOffset")) return false;
    if(in.contains("A_yOffset")) { const auto& v=in["A_yOffset"]; if(!v.is_number()) return false; o["A_yOffset"]=v; }
    if(!in.contains("A_zOffset")) return false;
    if(in.contains("A_zOffset")) { const auto& v=in["A_zOffset"]; if(!v.is_number()) return false; o["A_zOffset"]=v; }
    out=std::move(o);
    return true;
}

// validate T_LinearSpeed3DType
static bool validate_T_LinearSpeed3DType(const json& in, json& out){
    if(!in.is_object()) return false;
    static const std::unordered_set<std::string> __keys = {"A_xSpeed", "A_ySpeed", "A_zSpeed"};
    if(!only_keys(in, __keys)) return false;
    json o=json::object();
    if(!in.contains("A_xSpeed")) return false;
    if(in.contains("A_xSpeed")) { const auto& v=in["A_xSpeed"]; if(!v.is_number()) return false; o["A_xSpeed"]=v; }
    if(!in.contains("A_ySpeed")) return false;
    if(in.contains("A_ySpeed")) { const auto& v=in["A_ySpeed"]; if(!v.is_number()) return false; o["A_ySpeed"]=v; }
    if(!in.contains("A_zSpeed")) return false;
    if(in.contains("A_zSpeed")) { const auto& v=in["A_zSpeed"]; if(!v.is_number()) return false; o["A_zSpeed"]=v; }
    out=std::move(o);
    return true;
}

// validate T_LinearVelocity2DType
static bool validate_T_LinearVelocity2DType(const json& in, json& out){
    if(!in.is_object()) return false;
    static const std::unordered_set<std::string> __keys = {"A_heading", "A_speed"};
    if(!only_keys(in, __keys)) return false;
    json o=json::object();
    if(!in.contains("A_heading")) return false;
    if(in.contains("A_heading")) { const auto& v=in["A_heading"]; if(!v.is_number()) return false; o["A_heading"]=v; }
    if(!in.contains("A_speed")) return false;
    if(in.contains("A_speed")) { const auto& v=in["A_speed"]; if(!v.is_number()) return false; o["A_speed"]=v; }
    out=std::move(o);
    return true;
}

// validate T_LinearVelocity3DType
static bool validate_T_LinearVelocity3DType(const json& in, json& out){
    if(!in.is_object()) return false;
    static const std::unordered_set<std::string> __keys = {"A_heading", "A_speed", "A_vrate"};
    if(!only_keys(in, __keys)) return false;
    json o=json::object();
    if(!in.contains("A_heading")) return false;
    if(in.contains("A_heading")) { const auto& v=in["A_heading"]; if(!v.is_number()) return false; o["A_heading"]=v; }
    if(!in.contains("A_speed")) return false;
    if(in.contains("A_speed")) { const auto& v=in["A_speed"]; if(!v.is_number()) return false; o["A_speed"]=v; }
    if(!in.contains("A_vrate")) return false;
    if(in.contains("A_vrate")) { const auto& v=in["A_vrate"]; if(!v.is_number()) return false; o["A_vrate"]=v; }
    out=std::move(o);
    return true;
}

// validate T_PointPolar3DType
static bool validate_T_PointPolar3DType(const json& in, json& out){
    if(!in.is_object()) return false;
    static const std::unordered_set<std::string> __keys = {"A_angle", "A_elevation", "A_radius"};
    if(!only_keys(in, __keys)) return false;
    json o=json::object();
    if(!in.contains("A_angle")) return false;
    if(in.contains("A_angle")) { const auto& v=in["A_angle"]; if(!v.is_number()) return false; o["A_angle"]=v; }
    if(!in.contains("A_elevation")) return false;
    if(in.contains("A_elevation")) { const auto& v=in["A_elevation"]; if(!v.is_number()) return false; o["A_elevation"]=v; }
    if(!in.contains("A_radius")) return false;
    if(in.contains("A_radius")) { const auto& v=in["A_radius"]; if(!v.is_number()) return false; o["A_radius"]=v; }
    out=std::move(o);
    return true;
}

// validate T_Position2DType
static bool validate_T_Position2DType(const json& in, json& out){
    if(!in.is_object()) return false;
    static const std::unordered_set<std::string> __keys = {"A_xPosition", "A_yPosition"};
    if(!only_keys(in, __keys)) return false;
    json o=json::object();
    if(!in.contains("A_xPosition")) return false;
    if(in.contains("A_xPosition")) { const auto& v=in["A_xPosition"]; if(!v.is_number_integer()) return false; o["A_xPosition"]=v; }
    if(!in.contains("A_yPosition")) return false;
    if(in.contains("A_yPosition")) { const auto& v=in["A_yPosition"]; if(!v.is_number_integer()) return false; o["A_yPosition"]=v; }
    out=std::move(o);
    return true;
}

// validate T_RotationalOffsetType
static bool validate_T_RotationalOffsetType(const json& in, json& out){
    if(!in.is_object()) return false;
    static const std::unordered_set<std::string> __keys = {"A_pitchOffset", "A_rollOffset", "A_yawOffset"};
    if(!only_keys(in, __keys)) return false;
    json o=json::object();
    if(!in.contains("A_pitchOffset")) return false;
    if(in.contains("A_pitchOffset")) { const auto& v=in["A_pitchOffset"]; if(!v.is_number()) return false; o["A_pitchOffset"]=v; }
    if(!in.contains("A_rollOffset")) return false;
    if(in.contains("A_rollOffset")) { const auto& v=in["A_rollOffset"]; if(!v.is_number()) return false; o["A_rollOffset"]=v; }
    if(!in.contains("A_yawOffset")) return false;
    if(in.contains("A_yawOffset")) { const auto& v=in["A_yawOffset"]; if(!v.is_number()) return false; o["A_yawOffset"]=v; }
    out=std::move(o);
    return true;
}

// validate T_Size2DType
static bool validate_T_Size2DType(const json& in, json& out){
    if(!in.is_object()) return false;
    static const std::unordered_set<std::string> __keys = {"A_xSize", "A_ySize"};
    if(!only_keys(in, __keys)) return false;
    json o=json::object();
    if(!in.contains("A_xSize")) return false;
    if(in.contains("A_xSize")) { const auto& v=in["A_xSize"]; if(!v.is_number_integer()) return false; o["A_xSize"]=v; }
    if(!in.contains("A_ySize")) return false;
    if(in.contains("A_ySize")) { const auto& v=in["A_ySize"]; if(!v.is_number_integer()) return false; o["A_ySize"]=v; }
    out=std::move(o);
    return true;
}

// handlers for C_Crew_Role_In_Mission_State
static bool handle_C_Crew_Role_In_Mission_State_from(const json& in, std::string& out){ json o; if(!validate_C_Crew_Role_In_Mission_State(in,o)) return false; out=o.dump(); return true; }
static bool handle_C_Crew_Role_In_Mission_State_to  (const json& in, std::string& out){ json o; if(!validate_C_Crew_Role_In_Mission_State(in,o)) return false; out=o.dump(); return true; }

// handlers for C_Alarm_Category_Specification
static bool handle_C_Alarm_Category_Specification_from(const json& in, std::string& out){ json o; if(!validate_C_Alarm_Category_Specification(in,o)) return false; out=o.dump(); return true; }
static bool handle_C_Alarm_Category_Specification_to  (const json& in, std::string& out){ json o; if(!validate_C_Alarm_Category_Specification(in,o)) return false; out=o.dump(); return true; }

// handlers for C_Mission_State_setMissionState
static bool handle_C_Mission_State_setMissionState_from(const json& in, std::string& out){ json o; if(!validate_C_Mission_State_setMissionState(in,o)) return false; out=o.dump(); return true; }
static bool handle_C_Mission_State_setMissionState_to  (const json& in, std::string& out){ json o; if(!validate_C_Mission_State_setMissionState(in,o)) return false; out=o.dump(); return true; }

// handlers for C_Mission_State
static bool handle_C_Mission_State_from(const json& in, std::string& out){ json o; if(!validate_C_Mission_State(in,o)) return false; out=o.dump(); return true; }
static bool handle_C_Mission_State_to  (const json& in, std::string& out){ json o; if(!validate_C_Mission_State(in,o)) return false; out=o.dump(); return true; }

// handlers for C_Actual_Alarm_acknowledgeAlarm
static bool handle_C_Actual_Alarm_acknowledgeAlarm_from(const json& in, std::string& out){ json o; if(!validate_C_Actual_Alarm_acknowledgeAlarm(in,o)) return false; out=o.dump(); return true; }
static bool handle_C_Actual_Alarm_acknowledgeAlarm_to  (const json& in, std::string& out){ json o; if(!validate_C_Actual_Alarm_acknowledgeAlarm(in,o)) return false; out=o.dump(); return true; }

// handlers for C_Actual_Alarm
static bool handle_C_Actual_Alarm_from(const json& in, std::string& out){ json o; if(!validate_C_Actual_Alarm(in,o)) return false; out=o.dump(); return true; }
static bool handle_C_Actual_Alarm_to  (const json& in, std::string& out){ json o; if(!validate_C_Actual_Alarm(in,o)) return false; out=o.dump(); return true; }

// handlers for C_Alarm_Condition_Specification_raiseAlarmCondition
static bool handle_C_Alarm_Condition_Specification_raiseAlarmCondition_from(const json& in, std::string& out){ json o; if(!validate_C_Alarm_Condition_Specification_raiseAlarmCondition(in,o)) return false; out=o.dump(); return true; }
static bool handle_C_Alarm_Condition_Specification_raiseAlarmCondition_to  (const json& in, std::string& out){ json o; if(!validate_C_Alarm_Condition_Specification_raiseAlarmCondition(in,o)) return false; out=o.dump(); return true; }

// handlers for C_Alarm_Condition_Specification_isOfInterestToCrewRole
static bool handle_C_Alarm_Condition_Specification_isOfInterestToCrewRole_from(const json& in, std::string& out){ json o; if(!validate_C_Alarm_Condition_Specification_isOfInterestToCrewRole(in,o)) return false; out=o.dump(); return true; }
static bool handle_C_Alarm_Condition_Specification_isOfInterestToCrewRole_to  (const json& in, std::string& out){ json o; if(!validate_C_Alarm_Condition_Specification_isOfInterestToCrewRole(in,o)) return false; out=o.dump(); return true; }

// handlers for C_Alarm_Condition_Specification
static bool handle_C_Alarm_Condition_Specification_from(const json& in, std::string& out){ json o; if(!validate_C_Alarm_Condition_Specification(in,o)) return false; out=o.dump(); return true; }
static bool handle_C_Alarm_Condition_Specification_to  (const json& in, std::string& out){ json o; if(!validate_C_Alarm_Condition_Specification(in,o)) return false; out=o.dump(); return true; }

// handlers for C_Tone_Specification
static bool handle_C_Tone_Specification_from(const json& in, std::string& out){ json o; if(!validate_C_Tone_Specification(in,o)) return false; out=o.dump(); return true; }
static bool handle_C_Tone_Specification_to  (const json& in, std::string& out){ json o; if(!validate_C_Tone_Specification(in,o)) return false; out=o.dump(); return true; }

// handlers for C_Own_Platform
static bool handle_C_Own_Platform_from(const json& in, std::string& out){ json o; if(!validate_C_Own_Platform(in,o)) return false; out=o.dump(); return true; }
static bool handle_C_Own_Platform_to  (const json& in, std::string& out){ json o; if(!validate_C_Own_Platform(in,o)) return false; out=o.dump(); return true; }

// handlers for C_Actual_Alarm_Condition_unoverrideAlarmCondition
static bool handle_C_Actual_Alarm_Condition_unoverrideAlarmCondition_from(const json& in, std::string& out){ json o; if(!validate_C_Actual_Alarm_Condition_unoverrideAlarmCondition(in,o)) return false; out=o.dump(); return true; }
static bool handle_C_Actual_Alarm_Condition_unoverrideAlarmCondition_to  (const json& in, std::string& out){ json o; if(!validate_C_Actual_Alarm_Condition_unoverrideAlarmCondition(in,o)) return false; out=o.dump(); return true; }

// handlers for C_Actual_Alarm_Condition_overrideAlarmCondition
static bool handle_C_Actual_Alarm_Condition_overrideAlarmCondition_from(const json& in, std::string& out){ json o; if(!validate_C_Actual_Alarm_Condition_overrideAlarmCondition(in,o)) return false; out=o.dump(); return true; }
static bool handle_C_Actual_Alarm_Condition_overrideAlarmCondition_to  (const json& in, std::string& out){ json o; if(!validate_C_Actual_Alarm_Condition_overrideAlarmCondition(in,o)) return false; out=o.dump(); return true; }

// handlers for C_Actual_Alarm_Condition_clearAlarmCondition
static bool handle_C_Actual_Alarm_Condition_clearAlarmCondition_from(const json& in, std::string& out){ json o; if(!validate_C_Actual_Alarm_Condition_clearAlarmCondition(in,o)) return false; out=o.dump(); return true; }
static bool handle_C_Actual_Alarm_Condition_clearAlarmCondition_to  (const json& in, std::string& out){ json o; if(!validate_C_Actual_Alarm_Condition_clearAlarmCondition(in,o)) return false; out=o.dump(); return true; }

// handlers for C_Actual_Alarm_Condition
static bool handle_C_Actual_Alarm_Condition_from(const json& in, std::string& out){ json o; if(!validate_C_Actual_Alarm_Condition(in,o)) return false; out=o.dump(); return true; }
static bool handle_C_Actual_Alarm_Condition_to  (const json& in, std::string& out){ json o; if(!validate_C_Actual_Alarm_Condition(in,o)) return false; out=o.dump(); return true; }

// handlers for C_Alarm_Category
static bool handle_C_Alarm_Category_from(const json& in, std::string& out){ json o; if(!validate_C_Alarm_Category(in,o)) return false; out=o.dump(); return true; }
static bool handle_C_Alarm_Category_to  (const json& in, std::string& out){ json o; if(!validate_C_Alarm_Category(in,o)) return false; out=o.dump(); return true; }

// handlers for P_Alarms_PSM::C_Crew_Role_In_Mission_State
static bool handle_P_Alarms_PSM__C_Crew_Role_In_Mission_State_from(const json& in, std::string& out){ json o; if(!validate_P_Alarms_PSM__C_Crew_Role_In_Mission_State(in,o)) return false; out=o.dump(); return true; }
static bool handle_P_Alarms_PSM__C_Crew_Role_In_Mission_State_to  (const json& in, std::string& out){ json o; if(!validate_P_Alarms_PSM__C_Crew_Role_In_Mission_State(in,o)) return false; out=o.dump(); return true; }

// handlers for P_Alarms_PSM::C_Alarm_Category_Specification
static bool handle_P_Alarms_PSM__C_Alarm_Category_Specification_from(const json& in, std::string& out){ json o; if(!validate_P_Alarms_PSM__C_Alarm_Category_Specification(in,o)) return false; out=o.dump(); return true; }
static bool handle_P_Alarms_PSM__C_Alarm_Category_Specification_to  (const json& in, std::string& out){ json o; if(!validate_P_Alarms_PSM__C_Alarm_Category_Specification(in,o)) return false; out=o.dump(); return true; }

// handlers for P_Alarms_PSM::C_Mission_State_setMissionState
static bool handle_P_Alarms_PSM__C_Mission_State_setMissionState_from(const json& in, std::string& out){ json o; if(!validate_P_Alarms_PSM__C_Mission_State_setMissionState(in,o)) return false; out=o.dump(); return true; }
static bool handle_P_Alarms_PSM__C_Mission_State_setMissionState_to  (const json& in, std::string& out){ json o; if(!validate_P_Alarms_PSM__C_Mission_State_setMissionState(in,o)) return false; out=o.dump(); return true; }

// handlers for P_Alarms_PSM::C_Mission_State
static bool handle_P_Alarms_PSM__C_Mission_State_from(const json& in, std::string& out){ json o; if(!validate_P_Alarms_PSM__C_Mission_State(in,o)) return false; out=o.dump(); return true; }
static bool handle_P_Alarms_PSM__C_Mission_State_to  (const json& in, std::string& out){ json o; if(!validate_P_Alarms_PSM__C_Mission_State(in,o)) return false; out=o.dump(); return true; }

// handlers for P_Alarms_PSM::C_Actual_Alarm_acknowledgeAlarm
static bool handle_P_Alarms_PSM__C_Actual_Alarm_acknowledgeAlarm_from(const json& in, std::string& out){ json o; if(!validate_P_Alarms_PSM__C_Actual_Alarm_acknowledgeAlarm(in,o)) return false; out=o.dump(); return true; }
static bool handle_P_Alarms_PSM__C_Actual_Alarm_acknowledgeAlarm_to  (const json& in, std::string& out){ json o; if(!validate_P_Alarms_PSM__C_Actual_Alarm_acknowledgeAlarm(in,o)) return false; out=o.dump(); return true; }

// handlers for P_Alarms_PSM::C_Actual_Alarm
static bool handle_P_Alarms_PSM__C_Actual_Alarm_from(const json& in, std::string& out){ json o; if(!validate_P_Alarms_PSM__C_Actual_Alarm(in,o)) return false; out=o.dump(); return true; }
static bool handle_P_Alarms_PSM__C_Actual_Alarm_to  (const json& in, std::string& out){ json o; if(!validate_P_Alarms_PSM__C_Actual_Alarm(in,o)) return false; out=o.dump(); return true; }

// handlers for P_Alarms_PSM::C_Alarm_Condition_Specification_raiseAlarmCondition
static bool handle_P_Alarms_PSM__C_Alarm_Condition_Specification_raiseAlarmCondition_from(const json& in, std::string& out){ json o; if(!validate_P_Alarms_PSM__C_Alarm_Condition_Specification_raiseAlarmCondition(in,o)) return false; out=o.dump(); return true; }
static bool handle_P_Alarms_PSM__C_Alarm_Condition_Specification_raiseAlarmCondition_to  (const json& in, std::string& out){ json o; if(!validate_P_Alarms_PSM__C_Alarm_Condition_Specification_raiseAlarmCondition(in,o)) return false; out=o.dump(); return true; }

// handlers for P_Alarms_PSM::C_Alarm_Condition_Specification_isOfInterestToCrewRole
static bool handle_P_Alarms_PSM__C_Alarm_Condition_Specification_isOfInterestToCrewRole_from(const json& in, std::string& out){ json o; if(!validate_P_Alarms_PSM__C_Alarm_Condition_Specification_isOfInterestToCrewRole(in,o)) return false; out=o.dump(); return true; }
static bool handle_P_Alarms_PSM__C_Alarm_Condition_Specification_isOfInterestToCrewRole_to  (const json& in, std::string& out){ json o; if(!validate_P_Alarms_PSM__C_Alarm_Condition_Specification_isOfInterestToCrewRole(in,o)) return false; out=o.dump(); return true; }

// handlers for P_Alarms_PSM::C_Alarm_Condition_Specification
static bool handle_P_Alarms_PSM__C_Alarm_Condition_Specification_from(const json& in, std::string& out){ json o; if(!validate_P_Alarms_PSM__C_Alarm_Condition_Specification(in,o)) return false; out=o.dump(); return true; }
static bool handle_P_Alarms_PSM__C_Alarm_Condition_Specification_to  (const json& in, std::string& out){ json o; if(!validate_P_Alarms_PSM__C_Alarm_Condition_Specification(in,o)) return false; out=o.dump(); return true; }

// handlers for P_Alarms_PSM::C_Tone_Specification
static bool handle_P_Alarms_PSM__C_Tone_Specification_from(const json& in, std::string& out){ json o; if(!validate_P_Alarms_PSM__C_Tone_Specification(in,o)) return false; out=o.dump(); return true; }
static bool handle_P_Alarms_PSM__C_Tone_Specification_to  (const json& in, std::string& out){ json o; if(!validate_P_Alarms_PSM__C_Tone_Specification(in,o)) return false; out=o.dump(); return true; }

// handlers for P_Alarms_PSM::C_Own_Platform
static bool handle_P_Alarms_PSM__C_Own_Platform_from(const json& in, std::string& out){ json o; if(!validate_P_Alarms_PSM__C_Own_Platform(in,o)) return false; out=o.dump(); return true; }
static bool handle_P_Alarms_PSM__C_Own_Platform_to  (const json& in, std::string& out){ json o; if(!validate_P_Alarms_PSM__C_Own_Platform(in,o)) return false; out=o.dump(); return true; }

// handlers for P_Alarms_PSM::C_Actual_Alarm_Condition_unoverrideAlarmCondition
static bool handle_P_Alarms_PSM__C_Actual_Alarm_Condition_unoverrideAlarmCondition_from(const json& in, std::string& out){ json o; if(!validate_P_Alarms_PSM__C_Actual_Alarm_Condition_unoverrideAlarmCondition(in,o)) return false; out=o.dump(); return true; }
static bool handle_P_Alarms_PSM__C_Actual_Alarm_Condition_unoverrideAlarmCondition_to  (const json& in, std::string& out){ json o; if(!validate_P_Alarms_PSM__C_Actual_Alarm_Condition_unoverrideAlarmCondition(in,o)) return false; out=o.dump(); return true; }

// handlers for P_Alarms_PSM::C_Actual_Alarm_Condition_overrideAlarmCondition
static bool handle_P_Alarms_PSM__C_Actual_Alarm_Condition_overrideAlarmCondition_from(const json& in, std::string& out){ json o; if(!validate_P_Alarms_PSM__C_Actual_Alarm_Condition_overrideAlarmCondition(in,o)) return false; out=o.dump(); return true; }
static bool handle_P_Alarms_PSM__C_Actual_Alarm_Condition_overrideAlarmCondition_to  (const json& in, std::string& out){ json o; if(!validate_P_Alarms_PSM__C_Actual_Alarm_Condition_overrideAlarmCondition(in,o)) return false; out=o.dump(); return true; }

// handlers for P_Alarms_PSM::C_Actual_Alarm_Condition_clearAlarmCondition
static bool handle_P_Alarms_PSM__C_Actual_Alarm_Condition_clearAlarmCondition_from(const json& in, std::string& out){ json o; if(!validate_P_Alarms_PSM__C_Actual_Alarm_Condition_clearAlarmCondition(in,o)) return false; out=o.dump(); return true; }
static bool handle_P_Alarms_PSM__C_Actual_Alarm_Condition_clearAlarmCondition_to  (const json& in, std::string& out){ json o; if(!validate_P_Alarms_PSM__C_Actual_Alarm_Condition_clearAlarmCondition(in,o)) return false; out=o.dump(); return true; }

// handlers for P_Alarms_PSM::C_Actual_Alarm_Condition
static bool handle_P_Alarms_PSM__C_Actual_Alarm_Condition_from(const json& in, std::string& out){ json o; if(!validate_P_Alarms_PSM__C_Actual_Alarm_Condition(in,o)) return false; out=o.dump(); return true; }
static bool handle_P_Alarms_PSM__C_Actual_Alarm_Condition_to  (const json& in, std::string& out){ json o; if(!validate_P_Alarms_PSM__C_Actual_Alarm_Condition(in,o)) return false; out=o.dump(); return true; }

// handlers for P_Alarms_PSM::C_Alarm_Category
static bool handle_P_Alarms_PSM__C_Alarm_Category_from(const json& in, std::string& out){ json o; if(!validate_P_Alarms_PSM__C_Alarm_Category(in,o)) return false; out=o.dump(); return true; }
static bool handle_P_Alarms_PSM__C_Alarm_Category_to  (const json& in, std::string& out){ json o; if(!validate_P_Alarms_PSM__C_Alarm_Category(in,o)) return false; out=o.dump(); return true; }

// handlers for AlarmMsg
static bool handle_AlarmMsg_from(const json& in, std::string& out){ json o; if(!validate_AlarmMsg(in,o)) return false; out=o.dump(); return true; }
static bool handle_AlarmMsg_to  (const json& in, std::string& out){ json o; if(!validate_AlarmMsg(in,o)) return false; out=o.dump(); return true; }

// handlers for StringMsg
static bool handle_StringMsg_from(const json& in, std::string& out){ json o; if(!validate_StringMsg(in,o)) return false; out=o.dump(); return true; }
static bool handle_StringMsg_to  (const json& in, std::string& out){ json o; if(!validate_StringMsg(in,o)) return false; out=o.dump(); return true; }

void register_handlers(std::unordered_map<std::string, std::pair<Handler,Handler>>& reg){
    reg["C_Crew_Role_In_Mission_State"] = {handle_C_Crew_Role_In_Mission_State_from, handle_C_Crew_Role_In_Mission_State_to};
    reg["C_Alarm_Category_Specification"] = {handle_C_Alarm_Category_Specification_from, handle_C_Alarm_Category_Specification_to};
    reg["C_Mission_State_setMissionState"] = {handle_C_Mission_State_setMissionState_from, handle_C_Mission_State_setMissionState_to};
    reg["C_Mission_State"] = {handle_C_Mission_State_from, handle_C_Mission_State_to};
    reg["C_Actual_Alarm_acknowledgeAlarm"] = {handle_C_Actual_Alarm_acknowledgeAlarm_from, handle_C_Actual_Alarm_acknowledgeAlarm_to};
    reg["C_Actual_Alarm"] = {handle_C_Actual_Alarm_from, handle_C_Actual_Alarm_to};
    reg["C_Alarm_Condition_Specification_raiseAlarmCondition"] = {handle_C_Alarm_Condition_Specification_raiseAlarmCondition_from, handle_C_Alarm_Condition_Specification_raiseAlarmCondition_to};
    reg["C_Alarm_Condition_Specification_isOfInterestToCrewRole"] = {handle_C_Alarm_Condition_Specification_isOfInterestToCrewRole_from, handle_C_Alarm_Condition_Specification_isOfInterestToCrewRole_to};
    reg["C_Alarm_Condition_Specification"] = {handle_C_Alarm_Condition_Specification_from, handle_C_Alarm_Condition_Specification_to};
    reg["C_Tone_Specification"] = {handle_C_Tone_Specification_from, handle_C_Tone_Specification_to};
    reg["C_Own_Platform"] = {handle_C_Own_Platform_from, handle_C_Own_Platform_to};
    reg["C_Actual_Alarm_Condition_unoverrideAlarmCondition"] = {handle_C_Actual_Alarm_Condition_unoverrideAlarmCondition_from, handle_C_Actual_Alarm_Condition_unoverrideAlarmCondition_to};
    reg["C_Actual_Alarm_Condition_overrideAlarmCondition"] = {handle_C_Actual_Alarm_Condition_overrideAlarmCondition_from, handle_C_Actual_Alarm_Condition_overrideAlarmCondition_to};
    reg["C_Actual_Alarm_Condition_clearAlarmCondition"] = {handle_C_Actual_Alarm_Condition_clearAlarmCondition_from, handle_C_Actual_Alarm_Condition_clearAlarmCondition_to};
    reg["C_Actual_Alarm_Condition"] = {handle_C_Actual_Alarm_Condition_from, handle_C_Actual_Alarm_Condition_to};
    reg["C_Alarm_Category"] = {handle_C_Alarm_Category_from, handle_C_Alarm_Category_to};
    reg["P_Alarms_PSM::C_Crew_Role_In_Mission_State"] = {handle_P_Alarms_PSM__C_Crew_Role_In_Mission_State_from, handle_P_Alarms_PSM__C_Crew_Role_In_Mission_State_to};
    reg["P_Alarms_PSM::C_Alarm_Category_Specification"] = {handle_P_Alarms_PSM__C_Alarm_Category_Specification_from, handle_P_Alarms_PSM__C_Alarm_Category_Specification_to};
    reg["P_Alarms_PSM::C_Mission_State_setMissionState"] = {handle_P_Alarms_PSM__C_Mission_State_setMissionState_from, handle_P_Alarms_PSM__C_Mission_State_setMissionState_to};
    reg["P_Alarms_PSM::C_Mission_State"] = {handle_P_Alarms_PSM__C_Mission_State_from, handle_P_Alarms_PSM__C_Mission_State_to};
    reg["P_Alarms_PSM::C_Actual_Alarm_acknowledgeAlarm"] = {handle_P_Alarms_PSM__C_Actual_Alarm_acknowledgeAlarm_from, handle_P_Alarms_PSM__C_Actual_Alarm_acknowledgeAlarm_to};
    reg["P_Alarms_PSM::C_Actual_Alarm"] = {handle_P_Alarms_PSM__C_Actual_Alarm_from, handle_P_Alarms_PSM__C_Actual_Alarm_to};
    reg["P_Alarms_PSM::C_Alarm_Condition_Specification_raiseAlarmCondition"] = {handle_P_Alarms_PSM__C_Alarm_Condition_Specification_raiseAlarmCondition_from, handle_P_Alarms_PSM__C_Alarm_Condition_Specification_raiseAlarmCondition_to};
    reg["P_Alarms_PSM::C_Alarm_Condition_Specification_isOfInterestToCrewRole"] = {handle_P_Alarms_PSM__C_Alarm_Condition_Specification_isOfInterestToCrewRole_from, handle_P_Alarms_PSM__C_Alarm_Condition_Specification_isOfInterestToCrewRole_to};
    reg["P_Alarms_PSM::C_Alarm_Condition_Specification"] = {handle_P_Alarms_PSM__C_Alarm_Condition_Specification_from, handle_P_Alarms_PSM__C_Alarm_Condition_Specification_to};
    reg["P_Alarms_PSM::C_Tone_Specification"] = {handle_P_Alarms_PSM__C_Tone_Specification_from, handle_P_Alarms_PSM__C_Tone_Specification_to};
    reg["P_Alarms_PSM::C_Own_Platform"] = {handle_P_Alarms_PSM__C_Own_Platform_from, handle_P_Alarms_PSM__C_Own_Platform_to};
    reg["P_Alarms_PSM::C_Actual_Alarm_Condition_unoverrideAlarmCondition"] = {handle_P_Alarms_PSM__C_Actual_Alarm_Condition_unoverrideAlarmCondition_from, handle_P_Alarms_PSM__C_Actual_Alarm_Condition_unoverrideAlarmCondition_to};
    reg["P_Alarms_PSM::C_Actual_Alarm_Condition_overrideAlarmCondition"] = {handle_P_Alarms_PSM__C_Actual_Alarm_Condition_overrideAlarmCondition_from, handle_P_Alarms_PSM__C_Actual_Alarm_Condition_overrideAlarmCondition_to};
    reg["P_Alarms_PSM::C_Actual_Alarm_Condition_clearAlarmCondition"] = {handle_P_Alarms_PSM__C_Actual_Alarm_Condition_clearAlarmCondition_from, handle_P_Alarms_PSM__C_Actual_Alarm_Condition_clearAlarmCondition_to};
    reg["P_Alarms_PSM::C_Actual_Alarm_Condition"] = {handle_P_Alarms_PSM__C_Actual_Alarm_Condition_from, handle_P_Alarms_PSM__C_Actual_Alarm_Condition_to};
    reg["P_Alarms_PSM::C_Alarm_Category"] = {handle_P_Alarms_PSM__C_Alarm_Category_from, handle_P_Alarms_PSM__C_Alarm_Category_to};
    reg["AlarmMsg"] = {handle_AlarmMsg_from, handle_AlarmMsg_to};
    reg["StringMsg"] = {handle_StringMsg_from, handle_StringMsg_to};
}
