#pragma once
#include <string>
#include <unordered_set>

// 생성됨: enum 허용 토큰 집합

static const std::unordered_set<std::string> kEnum_P_Alarms_PSM__T_Actual_Alarm_StateType = {"L_Actual_Alarm_StateType_Unacknowledged", "L_Actual_Alarm_StateType_Acknowledged", "L_Actual_Alarm_StateType_Resolved", "L_Actual_Alarm_StateType_Destroyed", "L_Actual_Alarm_StateType_Cleared"};
static const std::unordered_set<std::string> kEnum_P_Alarms_PSM__T_AlarmCategoryType = {"L_AlarmCategoryType_Warning", "L_AlarmCategoryType_Caution", "L_AlarmCategoryType_Advisory"};
static const std::unordered_set<std::string> kEnum_P_Alarms_PSM__T_Alarm_Condition_Specification_StateType = {"L_Alarm_Condition_Specification_StateType_condition_overridden", "L_Alarm_Condition_Specification_StateType_condition_not_overridden"};
static const std::unordered_set<std::string> kEnum_P_Alarms_PSM__T_MissionStateType = {"L_MissionStateType_Driving", "L_MissionStateType_Engagement", "L_MissionStateType_Reconnaissance"};
static const std::unordered_set<std::string> kEnum_P_LDM_Common__T_Axis3DType = {"L_Axis3DType_X_AXIS", "L_Axis3DType_Y_AXIS", "L_Axis3DType_Z_AXIS"};
static const std::unordered_set<std::string> kEnum_P_LDM_Common__T_CommandResponseType = {"L_CommandResponseType_Command_Failed", "L_CommandResponseType_Command_Not_Available", "L_CommandResponseType_Command_Not_Recognised", "L_CommandResponseType_Resource_Control_Failure"};
static const std::unordered_set<std::string> kEnum_P_LDM_Common__T_RAGType = {"L_RAGType_RED", "L_RAGType_AMBER", "L_RAGType_GREEN"};
static const std::unordered_set<std::string> kEnum_T_Actual_Alarm_StateType = {"L_Actual_Alarm_StateType_Unacknowledged", "L_Actual_Alarm_StateType_Acknowledged", "L_Actual_Alarm_StateType_Resolved", "L_Actual_Alarm_StateType_Destroyed", "L_Actual_Alarm_StateType_Cleared"};
static const std::unordered_set<std::string> kEnum_T_AlarmCategoryType = {"L_AlarmCategoryType_Warning", "L_AlarmCategoryType_Caution", "L_AlarmCategoryType_Advisory"};
static const std::unordered_set<std::string> kEnum_T_Alarm_Condition_Specification_StateType = {"L_Alarm_Condition_Specification_StateType_condition_overridden", "L_Alarm_Condition_Specification_StateType_condition_not_overridden"};
static const std::unordered_set<std::string> kEnum_T_Axis3DType = {"L_Axis3DType_X_AXIS", "L_Axis3DType_Y_AXIS", "L_Axis3DType_Z_AXIS"};
static const std::unordered_set<std::string> kEnum_T_CommandResponseType = {"L_CommandResponseType_Command_Failed", "L_CommandResponseType_Command_Not_Available", "L_CommandResponseType_Command_Not_Recognised", "L_CommandResponseType_Resource_Control_Failure"};
static const std::unordered_set<std::string> kEnum_T_MissionStateType = {"L_MissionStateType_Driving", "L_MissionStateType_Engagement", "L_MissionStateType_Reconnaissance"};
static const std::unordered_set<std::string> kEnum_T_RAGType = {"L_RAGType_RED", "L_RAGType_AMBER", "L_RAGType_GREEN"};
