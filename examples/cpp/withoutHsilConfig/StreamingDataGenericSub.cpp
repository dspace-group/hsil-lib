/** <!-------------------------------------------------------------------------->
*
*   @file StreamingDataGenericSub.cpp
*
*   @brief HSIL CoSim StreamingData Subscriber example (without hsil config) for Generic Protocol
*
*   @author
*       dSPACE SE & Co. KG
*
*   @copyright
*       Copyright 2026, dSPACE SE & Co. KG. All rights reserved.
*
*   <hr><br>
*<!-------------------------------------------------------------------------->*/

#include <hsil/hsil.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <thread>

static void onStreamingData(const char* topicName, const HsilStreamingData* data, void* userData)
{
    (void)userData;
    std::printf("[SUB] topic=%s protocol=%s ts=%lu dataSize=%zu :: data='",
                topicName, data->protocol, data->timestamp, data->dataSize);
    
    /* Print payload safely assuming it's a printable string */
    for (size_t i = 0; i < data->dataSize; ++i)
    {
        if (data->data[i] >= 32 && data->data[i] <= 126)
        {
            std::putchar(data->data[i]);
        }
        else
        {
            std::printf("\\x%02X", data->data[i]);
        }
    }
    std::printf("'");

    /* Print generic metadata */
    std::printf(" metaSize=%zu metaData=[", data->meta.generic.size);
    for (size_t i = 0; i < data->meta.generic.size; ++i)
    {
        std::printf("%02X%s", data->meta.generic.data[i], (i + 1 < data->meta.generic.size) ? " " : "");
    }
    std::printf("]\n");
}

int main()
{
    int domainId = 42;  // must match publisher's domain
    const char* topicName = "GenericStreamTopic";   // must match publisher's topic
    HsilQos qos = HSIL_DEFAULT_QOS; // Use default QoS; can be customized if desired

    std::printf("=== HSIL StreamingData Subscriber - Generic (without hsil config) ===\n");

    std::printf("Domain: %d\n", domainId);
    std::printf("Topic Name: %s\n", topicName);
    std::printf("QoS Priority: %d\n", qos.priority);
    std::printf("QoS Queue Length: %d\n", qos.queueLength);
    std::printf("QoS Reliability: %d\n\n", qos.reliability);

    /* Create hsil session on the specified domain. */
    HsilHandle handle = nullptr;
    int rc = hsil_create(domainId, nullptr, &handle);
    if (rc != HSIL_OK) {
        std::fprintf(stderr, "Error: Failed to create HSIL session (err=%d).\n", rc);
        return EXIT_FAILURE;
    }

    /* Subscribe to the streaming topic with the specified QoS and callback.
       Callback will be invoked for each received streaming data frame. */
    rc = hsil_subscribe_streaming(handle, topicName, &qos, "generic", onStreamingData, nullptr);
    if (rc != HSIL_OK) {
        std::fprintf(stderr, "Error: Failed to subscribe to streaming topic (err=%d).\n", rc);
        hsil_destroy(handle);
        return EXIT_FAILURE;
    }

    std::printf("Subscribed! Listening for generic stream. Press Ctrl+C to terminate.\n\n");

    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    /* Destroy the hsil session. */
    hsil_destroy(handle);

    return EXIT_SUCCESS;
}
