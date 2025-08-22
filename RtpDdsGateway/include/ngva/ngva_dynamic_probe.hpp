#pragma once
#include "ndds/ndds_cpp.h"

// DynamicData 기반 제너릭 Reader/폴러: 타입 정의 없이 필드/값을 로그로 덤프
DDSDataReader* create_dynamic_reader(DDSSubscriber* sub, DDSTopic* topic);
void poll_dynamic_reader_once(DDSDataReader* rdr, int timeout_ms);
