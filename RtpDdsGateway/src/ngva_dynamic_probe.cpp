#include "ngva_dynamic_probe.hpp"
#include <cstdio>

static void dump_dynamic_data(DDS_DynamicData& d)
{
    // 단순 덤프: 멤버 인덱스 기반으로 이름/종류를 나열
    DDS_UnsignedLong count = d.get_member_count();
    for (DDS_UnsignedLong i=1; i<=count; ++i) {
        char name[128] = {0};
        DDS_DynamicDataMemberInfo info;
        if (d.get_member_info_by_index(info, i) != DDS_RETCODE_OK) continue;
        d.get_member_name_by_index(name, sizeof(name), i);
        // 값 종류에 따라 일부만 시연
        switch (info.member_kind) {
            case DDS_TK_LONG: {
                DDS_Long v=0; d.get_long(&v, name, DDS_DYNAMIC_DATA_MEMBER_ID_UNSPECIFIED);
                std::printf("  %s: %d\n", name, (int)v);
            } break;
            case DDS_TK_STRING: {
                char* s=nullptr; d.get_string(&s, name, DDS_DYNAMIC_DATA_MEMBER_ID_UNSPECIFIED);
                std::printf("  %s: %s\n", name, s?s:"<null>");
            } break;
            case DDS_TK_FLOAT: {
                DDS_Float f=0; d.get_float(&f, name, DDS_DYNAMIC_DATA_MEMBER_ID_UNSPECIFIED);
                std::printf("  %s: %f\n", name, (double)f);
            } break;
            default:
                std::printf("  %s: <kind=%d>\n", name, (int)info.member_kind);
        }
    }
}

DDSDataReader* create_dynamic_reader(DDSSubscriber* sub, DDSTopic* topic)
{
    // 타입 이름이 비어 있어도 DynamicData로 구독 가능(타입 전파가 되어야 함)
    DDS_DataReaderQos rqos;
    sub->get_default_datareader_qos(rqos);

    // 타입 호환↑ (필요 시 TypeConsistencyEnforcement 등 추가 설정 가능)
    // rqos.type_consistency.kind = ... (버전에 따라 사용)

    return sub->create_datareader(
        topic, rqos, NULL, DDS_STATUS_MASK_NONE);
}

void poll_dynamic_reader_once(DDSDataReader* rdr, int timeout_ms)
{
    if (!rdr) return;
    DDS_WaitSet ws;
    DDSStatusCondition* sc = rdr->get_statuscondition();
    sc->set_enabled_statuses(DDS_DATA_AVAILABLE_STATUS);
    ws.attach_condition(sc);

    DDS_Duration_t to = { timeout_ms/1000, (timeout_ms%1000)*1000000 };
    DDSConditionSeq active;
    if (ws.wait(active, to) != DDS_RETCODE_OK) {
        ws.detach_condition(sc);
        return;
    }
    ws.detach_condition(sc);

    // DynamicData로 take
    DDS_DynamicDataReader* dr = DDS_DynamicDataReader_narrow(rdr);
    if (!dr) return;

    DDS_DynamicDataSeq data_seq;
    DDS_SampleInfoSeq info_seq;

    if (dr->take(data_seq, info_seq, DDS_LENGTH_UNLIMITED,
                 DDS_ANY_SAMPLE_STATE, DDS_ANY_VIEW_STATE, DDS_ANY_INSTANCE_STATE) != DDS_RETCODE_OK) {
        return;
    }
    const int n = data_seq.length();
    for (int i=0; i<n; ++i) {
        if (!info_seq[i].valid_data) continue;
        std::printf("[NGVA][Dynamic] Sample #%d\n", i);
        dump_dynamic_data(data_seq[i]);
    }
    dr->return_loan(data_seq, info_seq);
}
