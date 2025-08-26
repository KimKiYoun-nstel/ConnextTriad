#pragma once
#ifdef USE_CONNEXT
#include "type_traits.hpp"
#include "type_registry.hpp"
#include "data_converter.hpp"

namespace rtpdds {

template <typename T> inline TypeRegistry::Entry make_entry() {
    return {
        [] { return TypeTraits<T>::name(); },
        [](void *p) { return TypeTraits<T>::register_type((DDSDomainParticipant *)p); },
        [](void *pub, void *topic) {
            DDSPublisher *publisher = (DDSPublisher *)pub;
            DDSTopic *t = (DDSTopic *)topic;
            DDS_DataWriterQos q;
            publisher->get_default_datawriter_qos(q);
            return (void *)publisher->create_datawriter(t, q, NULL, DDS_STATUS_MASK_NONE);
        },
        [](void *sub, void *topic) {
            DDSSubscriber *subscriber = (DDSSubscriber *)sub;
            DDSTopic *t = (DDSTopic *)topic;
            DDS_DataReaderQos q;
            subscriber->get_default_datareader_qos(q);
            return (void *)subscriber->create_datareader(t, q, NULL, DDS_STATUS_MASK_NONE);
        },
        [](void *w, const std::string &text) {
            auto *writer = (DDSDataWriter *)w;
            using DW = typename TypeTraits<T>::DataWriter;
            auto *narrow = DW::narrow(writer);
            if (!narrow)
                return false;
            T sample = DataConverter<T>::from_text(text);
            DDS_ReturnCode_t rc = narrow->write(sample, DDS_HANDLE_NIL);
            DataConverter<T>::cleanup(sample);
            return rc == DDS_RETCODE_OK;
        },
        [](void *r) {
            auto *reader = (DDSDataReader *)r;
            using DR = typename TypeTraits<T>::DataReader;
            auto *narrow = DR::narrow(reader);
            if (!narrow)
                return std::string();
            typename TypeTraits<T>::Seq data;
            DDS_SampleInfoSeq info;
            auto rc = narrow->take(data, info, 1, DDS_ANY_SAMPLE_STATE, DDS_ANY_VIEW_STATE, DDS_ANY_INSTANCE_STATE);
            if (rc != DDS_RETCODE_OK || data.length() == 0 || !info[0].valid_data) {
                if (rc == DDS_RETCODE_OK)
                    narrow->return_loan(data, info);
                return std::string();
            }
            std::string disp = DataConverter<T>::to_display(data[0]);
            narrow->return_loan(data, info);
            return disp;
        }
    };
}

#define REGISTER_MESSAGE_TYPE(REGISTRY, T) \
    do { (REGISTRY).by_name[TypeTraits<T>::name()] = make_entry<T>(); } while (0)

} // namespace rtpdds
#endif


