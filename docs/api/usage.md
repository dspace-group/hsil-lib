# API USAGE

The HSIL CoSim library operates in one of two ways depending on how you prefer to configure your co-simulation.

---

## 1. Without Hsil Configuration

The user must explicitly create publishers, subscribes to topics, and configure the environment at runtime. This is ideal for small projects with less topics and lesser signals to simulate. As the project grows, its hard to exchange the configuration information between multiple applications.

See (examples/withoutHsilConfig) for mor examples.

###  Example

```c
#include <hsil/hsil.h>
#include <stdio.h>
#include <string.h>

/* Callback for received GroupedData */
void onSensorData(const char* topicName, const HsilGroupedData* data, void* userData)
{
    printf("Topic %s: received ID %u, size %zu bytes\n",
           topicName, data->id, data->dataSize);
}

int main(void)
{
    /* 1. Open a session on DDS domain 42 */
    HsilHandle h;
    if (hsil_create(42, NULL, &h) != HSIL_OK) {
        return 1;
    }

    /* 2. Subscribe to a GroupedData topic (HSIL_ALL_IDS to receive all group IDs) */
    hsil_subscribe_grouped(h, "SensorTopic", NULL, HSIL_ALL_IDS, onSensorData, NULL);

    /* 3. Create a publisher for a StreamingData topic */
    hsil_create_streaming_publisher(h, "CAN_Bus", NULL);

    /* 4. Publish a streaming frame */
    HsilStreamingData frame;
    strncpy(frame.protocol, "CAN_v1", sizeof(frame.protocol));
    frame.timestamp = 1000000000ULL; // 1 second in ns
    
    uint8_t meta[] = { 0x01, 0x00 };
    uint8_t payload[] = { 0xDE, 0xAD, 0xBE, 0xEF };
    frame.meta = meta;
    frame.metaSize = sizeof(meta);
    frame.data = payload;
    frame.dataSize = sizeof(payload);

    hsil_publish_streaming(h, "CAN_Bus", &frame);

    /* 5. Clean up the session and all associated publishers/subscribers */
    hsil_destroy(h);
    return 0;
}
```

### Ownership Rules
- All DDS entities (readers/writers) are **owned by the session** and are freed automatically by `hsil_destroy()`.
- `hsil_subscribe_grouped` and `hsil_subscribe_streaming` create underlying readers on their first call and reuse them subsequently.
- `hsil_create_grouped_publisher`/`hsil_create_streaming_publisher` must be called once before publishing.

### Setting QoS
- When using HSIL API without the hsil configuration file, user can optionally provide QoS, specified in the HSIL Specification, while subscribing or creating publisher on a topic.

```c
/* set qos for the subscriber of a topic. */
HsilQos readerQos = HSIL_DEFAULT_QOS;
qos.reliability = HSIL_QOS_RELIABILITY_RELIABLE;
qos.queueLength = 1000;

hsil_subscribe_streaming(h, "StreamingTopic", &readerQos, "generic", onStreamCbk, NULL);
```

```c
/* set qos for the publisher of a topic. */
HsilQos writerQos = HSIL_DEFAULT_QOS;
qos.reliability = HSIL_QOS_RELIABILITY_RELIABLE;

hsil_create_streaming_publisher(h, "StreamingTopic", &writerQos);
```
*Note: The snippets above are intentionally left incomplete to only demonstrate setting QoS*

### Checking Endpoint Status
DDS discovery is asynchronous: a publisher created a moment ago has usually not met any subscriber yet, and samples published before a match exists are discarded. Use `hsil_get_publisher_status()` / `hsil_get_subscriber_status()` to see whether anyone is on the other end.

>**Note:** The example below is intentionally more complex to demonstrate the complete usage of endpoint status (matched status and qos mismatch). A simple matched status check is ideally sufficient in most of the use cases.
```c
/* Wait for a subscriber before publishing, so no sample is lost. */
for (int i = 0; i < 100; ++i) {
    HsilEndpointStatus status;
    if (hsil_get_publisher_status(h, "GroupedTopic", &status) != HSIL_OK) {
        break;
    }

    if (status.matchedCount > 0) {
        break;                    /* at least one subscriber is listening */
    }

    if (status.qosMismatchCount > 0 &&
        status.lastMismatchedQosPolicy == HSIL_QOS_MISMATCH_RELIABILITY) {
        /* A subscriber exists but requires RELIABLE while this publisher is FAST.
           Waiting longer will not help - the QoS has to be fixed. */
        fprintf(stderr, "Incompatible reliability QoS.\n");
        break;
    }

    sleepMs(100);
}
```

Both calls return immediately, so poll them rather than expecting them to block. `matchedCount` is a live count; `qosMismatchCount` is cumulative and never decreases. These calls work the same way with or without a hsil configuration file.

---

## 2. With Hsil Configuration

The session is derived using a JSON simulation configuration file, as specified by HSIL Specification, should be provided by the user. The library automatically reads/parses the configuration, joins the declared DDS domain, and pre-creates all readers and writers.

This is quite beneficial for bigger projects with more signals/ buses as they can simply be put in the configuration file and exchanged between different applications with minimal changes. 

For the GroupedData, convinience APIs hsil_read_signal_by_name()/ hsil_write_signal_by_name() to read signals from or write signals to the GroupedData packets are provided. Instead, user may choose to write their own serialization/ deserialization logic to write to or read from the GroupedData packets.

Please note that for the StreamingData no such convinience APIs are provived since the bus data are byte streams which can easily be written to or read from the StreamingData packets.

(See examples/bmsScenarioWithHsilConfig) for a full blown working example.

### Example

```c
#include <hsil/hsil.h>
#include <stdio.h>

/* Callback for received StreamingData */
void onCAN(const char* topicName, const HsilStreamingData* data, void* userData)
{
    printf("CAN frame on %s: %zu bytes\n", topicName, data->dataSize);
}

int main(void)
{
    /* 1. Load configuration: joins domain and pre-creates all DDS entities */
    HsilHandle h;
    if (hsil_create_from_config("simulation.json", NULL, &h) != HSIL_OK) {
        return 1;
    }

    /* 2. Attach a callback to a subscribed topic declared in the JSON */
    hsil_subscribe_streaming(h, "CAN_Bus", NULL, "CAN_v1", onCAN, NULL);

    /* 3. Prepare the data sample before publishing */
    HsilGroupedData sample;
    /* Use hsil_write_signal_by_name() to set the individual signals by name inside the GroupedData sample..
       (See example/bmsScenarioWithHsilConfig) for a full blown working example. */

    /* 4. Publish to a topic declared as published in the JSON */
    hsil_publish_grouped(h, "SensorTopic", &sample);

    /* 5. Clean up: frees all pre-created entities */
    hsil_destroy(h);
    return 0;
}
```

### Ownership and Validation Rules
- All DDS entities are pre-created during `hsil_create_from_config()`.
- `hsil_subscribe_*` and `hsil_publish_*` look up entities. If a topic was not declared in the config, they return `HSIL_ERR_UNKNOWN_TOPIC`.
- All resources are destroyed automatically by `hsil_destroy()`.

---

## 3. Using XML Profile to Configure DDS
A XML profile can optionally be passed while creating the hsil session to configure various DDS settings like Domain, Discovery, Multicasting, Transport protocol etc. See [Cyclone DDS Configuration Guide](https://cyclonedds.io/docs/cyclonedds/latest/config/index.html#)

Example without using hsil configuration file
```c
HsilHandle h;
const char* xmlConfigPath = "/absolute/path/to/xml/config";

hsil_create(42, xmlConfigPath, &h);
```

Example when using hsil configuration file
```c
HsilHandle h;
/* Note: For relative path: At runtime its relative to the path from which the application is run. */
const char* hsilConfigPath = "/relative/path/config/file";
const char* xmlConfigPath = "/relative/path/to/xml/config";

hsil_create_from_config(hsilConfigPath, xmlConfigPath, &h);
```

---

