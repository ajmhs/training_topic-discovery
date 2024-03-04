/*
* (c) Copyright, Real-Time Innovations, 2020.  All rights reserved.
* RTI grants Licensee a license to use, modify, compile, and create derivative
* works of the software solely for use with RTI Connext DDS. Licensee may
* redistribute copies of the software provided that all such copies are subject
* to this license. The software is provided "as is", with no warranty of any
* type, including any warranty for fitness for any purpose. RTI is under no
* obligation to maintain or support the software. RTI shall not be liable for
* any incidental or consequential damages arising out of the use or inability
* to use the software.
*/

#include <algorithm>
#include <iostream>

#include <dds/sub/ddssub.hpp>
#include <dds/core/ddscore.hpp>
#include <rti/config/Logger.hpp>  // for logging
// alternatively, to include all the standard APIs:
//  <dds/dds.hpp>
// or to include both the standard APIs and extensions:
//  <rti/rti.hpp>
//
// For more information about the headers and namespaces, see:
//    https://community.rti.com/static/documentation/connext-dds/7.2.0/doc/api/connext_dds/api_cpp2/group__DDSNamespaceModule.html
// For information on how to use extensions, see:
//    https://community.rti.com/static/documentation/connext-dds/7.2.0/doc/api/connext_dds/api_cpp2/group__DDSCpp2Conventions.html

#include "shapes.hpp"
#include "application.hpp"  // for command line parsing and ctrl-c
#include <string>
#include <sstream>

using std::cout;
using std::string;
using std::ostringstream;

static const char endl = '\n';
static const string shared_secret = "Now is the time for all good men to come to the aid of the party";

template <typename T>
class PublisherListener : public dds::sub::NoOpDataReaderListener<T> {

     // This gets called when a subscriber has been discovered
    void on_data_available(dds::sub::DataReader<T> &reader) {
        
        // Only process newly seen subscribers
        dds::sub::LoanedSamples<T> samples = reader.select().state(dds::sub::status::DataState::new_instance()).take();

        for (const auto &sample : samples) {
            if (!sample.info().valid())
                continue;            

            std::ios::fmtflags default_format(cout.flags());
            cout << std::hex << std::setw(8) << std::setfill('0');

            const dds::topic::BuiltinTopicKey &partKey = sample.data().participant_key();
            const dds::topic::BuiltinTopicKey &key = sample.data().key();
            cout << "Built-in Reader: found publisher" << endl
                      << "\tparticipant_key->'" << partKey.value()[0] << " "
                      << partKey.value()[1] << " " << partKey.value()[2] << "'"
                      << endl
                      << "\tkey->'" << key.value()[0] << " " << key.value()[1]
                      << " " << key.value()[2] << "'" << endl
                      << "\tinstance_handle: "
                      << sample.info().instance_handle() << endl;

            cout.flags(default_format);
        }
    }
};


int process_data(dds::sub::DataReader< ::ShapeTypeExtended> reader)
{
    // Take all samples
    int count = 0;
    dds::sub::LoanedSamples< ::ShapeTypeExtended> samples = reader.take();
    for (auto sample : samples) {
        if (sample.info().valid()) {
            count++;
            cout << sample.data() << endl;
        } else {
            cout << "Instance state changed to "
            << sample.info().state().instance_state() << endl;
        }
    }

    return count; 
} // The LoanedSamples destructor returns the loan

void run_subscriber_application(unsigned int domain_id, unsigned int sample_count)
{
    // DDS objects behave like shared pointers or value types
    // (see https://community.rti.com/best-practices/use-modern-c-types-correctly)

     // Retrieve the default participant QoS, from USER_QOS_PROFILES.xml
    dds::domain::qos::DomainParticipantQos participant_qos = dds::core::QosProvider::Default().participant_qos();
    auto resource_limits_qos = participant_qos.policy<rti::core::policy::DomainParticipantResourceLimits>();

    // Check the user_data length
    unsigned int max_len = resource_limits_qos.participant_user_data_max_length();
    if (shared_secret.size() > max_len) {
        cout << "error, participant user_data exceeds resource limits" << endl;
        return;
    } 
    else {
        // Hash the shared secret, copy it to a byte sequence and set it as the participant userdata         
        std::size_t hash = std::hash<string>{}(shared_secret);
        auto bytes = dds::core::ByteSeq((uint8_t*)&hash, (uint8_t*)&hash + sizeof(hash));
        participant_qos << dds::core::policy::UserData(bytes);
    }

    // Start communicating in a domain, usually one participant per application
    dds::domain::DomainParticipant participant(domain_id, participant_qos);

    // Get the built-in subscriber
    dds::sub::Subscriber builtin_subscriber = dds::sub::builtin_subscriber(participant);

    // Create shared pointer to PublisherListener class
    auto publisher_listener = std::make_shared<PublisherListener<dds::topic::PublicationBuiltinTopicData>>();

    // Get builtin subscriber's datareader for publishers.
    std::vector<dds::sub::DataReader<dds::topic::PublicationBuiltinTopicData>> publication_reader;
    dds::sub::find<dds::sub::DataReader<dds::topic::PublicationBuiltinTopicData>>(
        builtin_subscriber,
        dds::topic::publication_topic_name(),
        std::back_inserter(publication_reader));

    // Install our listener using the shared pointer
    publication_reader.front().set_listener(publisher_listener);

    // Now that the listener is installed, enable the participant now.
    participant.enable();

    // Create a Topic with a name and a datatype
    dds::topic::Topic< ::ShapeTypeExtended> topic(participant, "Triangle");

    // Create a Subscriber and DataReader with default Qos
    dds::sub::Subscriber subscriber(participant);
    dds::sub::DataReader< ::ShapeTypeExtended> reader(subscriber, topic);

    // Create a ReadCondition for any data received on this reader and set a
    // handler to process the data
    unsigned int samples_read = 0;
    dds::sub::cond::ReadCondition read_condition(
        reader,
        dds::sub::status::DataState::any(),
        [reader, &samples_read]() { samples_read += process_data(reader); });

    // WaitSet will be woken when the attached condition is triggered
    dds::core::cond::WaitSet waitset;
    waitset += read_condition;

    cout << "::ShapeTypeExtended subscriber ready..." << endl;
    while (!application::shutdown_requested && samples_read < sample_count) {
        
        // Run the handlers of the active conditions. Wait for up to 1 second.
        waitset.dispatch(dds::core::Duration(1));
    }
}

int main(int argc, char *argv[])
{

    using namespace application;

    // Parse arguments and handle control-C
    auto arguments = parse_arguments(argc, argv);
    if (arguments.parse_result == ParseReturn::exit) {
        return EXIT_SUCCESS;
    } else if (arguments.parse_result == ParseReturn::failure) {
        return EXIT_FAILURE;
    }
    setup_signal_handlers();

    // Sets Connext verbosity to help debugging
    rti::config::Logger::instance().verbosity(arguments.verbosity);

    try {
        run_subscriber_application(arguments.domain_id, arguments.sample_count);
    } catch (const std::exception& ex) {
        // This will catch DDS exceptions
        std::cerr << "Exception in run_subscriber_application(): " << ex.what()
        << endl;
        return EXIT_FAILURE;
    }

    // Releases the memory used by the participant factory.  Optional at
    // application exit
    dds::domain::DomainParticipant::finalize_participant_factory();

    return EXIT_SUCCESS;
}
