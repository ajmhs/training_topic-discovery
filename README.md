# Console application based on Shapes which demonstrates display of the discovery data for the 3 builtin readers

Skeleton created via
 
`rtiddsgen -language C++11 -platform x64Linux4gcc7.3.0 -example x64Linux4gcc7.3.0 -create makefiles -create typefiles -d c++11 shapes.idl`

## Common (QoS)
Changes to the Quality of Service were made to disable entities from being enabled automatically 

```xml
<participant_factory_qos>
    <entity_factory>
        <autoenable_created_entities>false</autoenable_created_entities>
    </entity_factory>
</participant_factory_qos>
```  

This allows the listeners for the builtin readers to be instantiated and connected to the entities before they start, permitting the capture of discovery data.

Additionally, this sample indicates a simple method of using a pre-shared secret to restrict subscription access. This is performed by setting a specific value in the subscriber's participant userdata. This userdata is read at discovery time. For sanity's sake the maximum user_data length is set to 1K

```xml
<resource_limits>
    <participant_user_data_max_length>1024</participant_user_data_max_length>
</resource_limits>
```  

## Publisher

The publisher defines two listeners; a ParticipantListener which implements a NoOpDataReaderListener<T> where T is ParticipantBuiltinTopicData and a SubscriberListener which also implements a NoOpDataReaderListener<T>, but where T is SubscriptionBuiltinTopicData. 

### ParticipantListener

The Participant listener via the `on_data_available` override uses a `select` call to "take" just the new instance samples, and then iterates over them to just operate on the valid samples. 

For each of those samples, representing a newly discovered participant, the user_data is extracted and converted from a byte vector to a `std::size_t` type. The BuiltinTopicKey data from the sample key is output to the console, along with the `user_data` as a "hash" received from the subscriber application. The hash should have a value of `0xdcff1a7b5c113752`.


### SubscriberListener

The subscriber listener, via the `on_data_available` override also performs the same `select` and valid iteration as the ParticipantListener. 

The data from the subscriber's `participant_key` and `key` are output to the console.
            
## Subscriber

The subscriber defines a single listener, a PublisherListener which is defined and attached to the builtin publisher listener prior to the participant being enabled. The publisher listener, via the `on_data_available` override uses a `select` call to "take" just the new instance samples, and then iterates over them to just operate on the valid samples. 

For each of those samples, representing a newly discovered publisher, the data from the publisher's `participant_key` and `key` are output to the console.

Before the participant being enabled, the `std::hash` value of the "shared secret" is calculated and written to a 
`ByteSeq` using the range content constructor. The contents of the `ByteSeq` are set as the participant's user data.
        
