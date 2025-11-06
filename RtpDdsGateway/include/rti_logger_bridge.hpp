#pragma once
#include <rti/config/Logger.hpp>

namespace rtpdds {
void InitRtiLoggerToTriad();                              // 한 번만 호출
void SetRtiLoggerVerbosity(rti::config::Verbosity v);     // 선택
}
