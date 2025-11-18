#include <iostream>
#include <chrono>
#include <thread>

#ifdef RTI_CONNEXT
#include <dds/dds.hpp>
#include "HelloWorld.hpp"
using namespace dds::domain;
using namespace dds::pub;
using namespace dds::topic;
using namespace dds::core;
#endif

int main(int argc, char** argv)
{
#ifdef RTI_CONNEXT
    // Create participant (domain 0)
    DomainParticipant participant(0);

    // Create topic and writer
    Topic<tutorial::HelloWorld> topic(participant, "HelloWorldTopic");
    Publisher pub(participant);
    DataWriter<tutorial::HelloWorld> writer(pub, topic);

    tutorial::HelloWorld sample;
    sample.message("Hello from VxWorks sample");

    for (int i = 0; i < 10; ++i) {
        writer.write(sample);
        std::cout << "Published: " << sample.message() << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    return 0;
#else
    std::cout << "RTI_CONNEXT not enabled. Build with USE_RTI=ON and NDDSHOME_CTL set." << std::endl;
    return 0;
#endif
}
