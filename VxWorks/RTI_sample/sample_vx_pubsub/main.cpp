#include <iostream>

#ifdef RTI_CONNEXT
#include <ndds/ndds_cpp.h>
#endif

int main(int argc, char** argv)
{
#ifdef RTI_CONNEXT
    DDS_DomainParticipantFactory* factory = DDS_DomainParticipantFactory::get_instance();
    if (factory == nullptr) {
        std::cerr << "Failed to get DomainParticipantFactory instance\n";
        return 1;
    }

    DDS_DomainParticipant* participant = factory->create_participant(
        0, DDS_PARTICIPANT_QOS_DEFAULT, nullptr, DDS_STATUS_MASK_NONE);
    if (participant == nullptr) {
        std::cerr << "create_participant failed\n";
        return 1;
    }

    std::cout << "Created DomainParticipant (domain 0)\n";

    DDS_ReturnCode_t rc = factory->delete_participant(participant);
    if (rc != DDS_RETCODE_OK) {
        std::cerr << "delete_participant failed: " << rc << "\n";
        return 1;
    }

    std::cout << "Cleaned up and exiting\n";
    return 0;
#else
    std::cout << "RTI_CONNEXT not enabled. Define USE_RTI=ON and ensure NDDS headers/libraries are available.\n";
    return 0;
#endif
}
