#include <iostream>
#include <thread>
#include <chrono>

#ifdef RTI_CONNEXT
#include <dds/dds.hpp>
#include "HelloWorld.hpp"
using namespace dds::domain;
using namespace dds::sub;
using namespace dds::topic;
using namespace dds::core;
#endif

int main(int argc, char** argv)
{
#ifdef RTI_CONNEXT
    DomainParticipant participant(0);
    Topic<tutorial::HelloWorld> topic(participant, "HelloWorldTopic");
    Subscriber sub(participant);
    DataReader<tutorial::HelloWorld> reader(sub, topic);

    for (int i = 0; i < 12; ++i) {
        dds::sub::LoanedSamples<tutorial::HelloWorld> samples = reader.take();
        if (samples.length() == 0) {
            std::cout << "No data" << std::endl;
        } else {
            for (auto& sample : samples) {
                if (sample.info().valid()) {
                    std::cout << "Received: " << sample.data().message() << std::endl;
                }
            }
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    return 0;
#else
    std::cout << "RTI_CONNEXT not enabled. Build with USE_RTI=ON and NDDSHOME_CTL set." << std::endl;
    return 0;
#endif
}
