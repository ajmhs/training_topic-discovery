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

#include <iostream>

#include <dds/sub/ddssub.hpp>
#include <dds/pub/ddspub.hpp>
#include <rti/util/util.hpp>      // for sleep()
#include <rti/config/Logger.hpp>  // for logging
#include <dds/domain/discovery.hpp>  // for ignore
// alternatively, to include all the standard APIs:
//  <dds/dds.hpp>
// or to include both the standard APIs and extensions:
//  <rti/rti.hpp>
//
// For more information about the headers and namespaces, see:
//    https://community.rti.com/static/documentation/connext-dds/7.2.0/doc/api/connext_dds/api_cpp2/group__DDSNamespaceModule.html
// For information on how to use extensions, see:
//    https://community.rti.com/static/documentation/connext-dds/7.2.0/doc/api/connext_dds/api_cpp2/group__DDSCpp2Conventions.html

#include "application.hpp"  // for command line parsing and ctrl-c
#include "shapes.hpp"
#include <string>

using std::cout;
using std::string;
static const char endl = '\n';

static const std::string shared_secret = "Now is the time for all good men to come to the aid of the party";
static const std::size_t secret_hash = std::hash<string>{}(shared_secret); 

template <typename T>
class ParticipantListener : public dds::sub::NoOpDataReaderListener<T> {

    // This gets called when a participant has been discovered
    void on_data_available(dds::sub::DataReader<T> &reader) {

        // Only process newly seen participants
        dds::sub::LoanedSamples<T> samples = reader.select()
                                    .state(dds::sub::status::DataState::new_instance())
                                    .take();

        for (const auto &sample : samples) {
            if (!sample.info().valid())
                continue;

            std::size_t user_hash = 0;
            const dds::core::ByteSeq &user_data = sample.data().user_data().value();            
            if (user_data.size() == sizeof(user_hash))
                std::copy((uint8_t*)&user_data[0], (uint8_t*)&user_data[0] + sizeof(std::size_t), (uint8_t*)&user_hash);

            std::ios::fmtflags default_format(cout.flags());
            cout << std::hex << std::setw(8) << std::setfill('0');

            const dds::topic::BuiltinTopicKey &key = sample.data().key();
            cout << "Built-in Reader: found participant" << endl
                      << "\tkey->'" << key.value()[0] << " " << key.value()[1] << " " << key.value()[2] << "'" << endl
                      << "\thash->'" << user_hash << "'" << endl
                      << "\tinstance_handle: " << sample.info().instance_handle() << endl;

            cout.flags(default_format);            
                
            if (user_hash != secret_hash) {
                cout << "Shared secrets do not match, ignoring participant" << endl;

                // Get the associated participant...
                dds::domain::DomainParticipant participant = reader.subscriber().participant();

                // Ignore the remote participant
                dds::domain::ignore(participant, sample->info().instance_handle());
            }
        }
    }
};

template <typename T>
class SubscriberListener : public dds::sub::NoOpDataReaderListener<T> {
                  
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
            cout << "Built-in Reader: found subscriber" << endl
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

void run_publisher_application(unsigned int domain_id, unsigned int sample_count)
{
    // DDS objects behave like shared pointers or value types
    // (see https://community.rti.com/best-practices/use-modern-c-types-correctly)

    // Start communicating in a domain, usually one participant per application
    dds::domain::DomainParticipant participant(domain_id);

    // Get the built-in subscriber
    dds::sub::Subscriber builtin_subscriber = dds::sub::builtin_subscriber(participant);

    // Create shared pointer to ParticipantListener class
    auto participant_listener = std::make_shared<ParticipantListener<dds::topic::ParticipantBuiltinTopicData>>();

    // Then get builtin subscriber's datareader for participants.
    std::vector<dds::sub::DataReader<dds::topic::ParticipantBuiltinTopicData>> participant_reader;
    dds::sub::find<dds::sub::DataReader<dds::topic::ParticipantBuiltinTopicData>>(
        builtin_subscriber,
        dds::topic::participant_topic_name(),
        std::back_inserter(participant_reader));

    participant_reader.front().set_listener(participant_listener);

    // Create shared pointer to SubscriberListener class
    auto subscriber_listener = std::make_shared<SubscriberListener<dds::topic::SubscriptionBuiltinTopicData>>();

     // Get builtin subscriber's datareader for subscribers.
    std::vector<dds::sub::DataReader<dds::topic::SubscriptionBuiltinTopicData>> subscription_reader;
    dds::sub::find<dds::sub::DataReader<dds::topic::SubscriptionBuiltinTopicData>>(
        builtin_subscriber,
        dds::topic::subscription_topic_name(),
        std::back_inserter(subscription_reader));

    // Install our listener using the shared pointer.
    subscription_reader.front().set_listener(subscriber_listener);

    // Now that all the listeners are installed, enable the participant now.
    participant.enable();

    // Create a Topic with a name and a datatype
    dds::topic::Topic< ::ShapeTypeExtended> topic(participant, "Triangle");

    // Create a Publisher
    dds::pub::Publisher publisher(participant);

    // Create a DataWriter with default QoS
    dds::pub::DataWriter< ::ShapeTypeExtended> writer(publisher, topic);

    ::ShapeTypeExtended data;
    // Main loop, write data
    for (unsigned int samples_written = 0;
    !application::shutdown_requested && samples_written < sample_count;
    samples_written++) {
        // Modify the data to be written here
        cout << "Writing ::ShapeTypeExtended, count " << samples_written << endl;

        writer.write(data);

        // Send once every second
        rti::util::sleep(dds::core::Duration(1));
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
        run_publisher_application(arguments.domain_id, arguments.sample_count);
    } catch (const std::exception& ex) {
        // This will catch DDS exceptions
        std::cerr << "Exception in run_publisher_application(): " << ex.what()
        << endl;
        return EXIT_FAILURE;
    }

    // Releases the memory used by the participant factory.  Optional at
    // application exit
    dds::domain::DomainParticipant::finalize_participant_factory();

    return EXIT_SUCCESS;
}
