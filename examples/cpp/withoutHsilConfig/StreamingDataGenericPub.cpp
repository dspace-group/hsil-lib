// SPDX-FileCopyrightText: 2026 dSPACE SE & Co. KG
// SPDX-License-Identifier: Apache-2.0

/** <!-------------------------------------------------------------------------->
*
*   @file StreamingDataGenericPub.cpp
*
*   @brief HSIL CoSim StreamingData Publisher example (without hsil config) for Generic Protocol
*
*   @author
*       dSPACE SE & Co. KG
*
*   @description
*       Publishes generic-protocol StreamingData samples on a topic without
*       using an HSIL JSON configuration file.
*
*   <hr><br>
*<!-------------------------------------------------------------------------->*/

#include <hsil/hsil.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <thread>

static uint64_t nowNs()
{
    using namespace std::chrono;
    return static_cast<uint64_t>(
        duration_cast<nanoseconds>(steady_clock::now().time_since_epoch()).count());
}

int main()
{
    int domainId = 42;  // must match subscriber's domain
    const char* topicName = "GenericStreamTopic";   // must match subscriber's topic
    HsilQos qos = HSIL_DEFAULT_QOS; // Use default QoS; can be customized if desired

    std::printf("=== HSIL StreamingData Publisher - Generic (without hsil config) ===\n");

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

    /* Create streaming publisher for the specified topic and qos. */
    rc = hsil_create_streaming_publisher(handle, topicName, &qos);
    if (rc != HSIL_OK) {
        std::fprintf(stderr, "Error: Failed to create streaming publisher (err=%d).\n", rc);
        hsil_destroy(handle);
        return EXIT_FAILURE;
    }

    std::printf("Publishing generic streaming data. Press Ctrl+C to terminate.\n\n");

    uint32_t cycle = 0;
    while (true) {
        /* Generic protocol payload and custom generic metadata. */
        uint8_t payload[16];
        std::snprintf(reinterpret_cast<char*>(payload), sizeof(payload), "GenData_%06u", cycle);

        /* Dummy generic metadata. */
        uint8_t metaBytes[] = { 0xAB, 0xCD, 0xEF, static_cast<uint8_t>(cycle & 0xFF) };

        HsilStreamingData frame{};
        std::strncpy(frame.protocol, "generic", sizeof(frame.protocol));
        frame.timestamp = nowNs();
        
        frame.meta.generic.size = sizeof(metaBytes);
        frame.meta.generic.data = metaBytes;
        
        frame.data = payload;
        frame.dataSize = sizeof(payload);

        /* Publish the streaming data frame. */
        rc = hsil_publish_streaming(handle, topicName, &frame);
        if (rc == HSIL_OK)
        {
            std::printf("[PUB] cycle=%u payload='%s' metaSize=%zu\n",
                        cycle, reinterpret_cast<char*>(payload), frame.meta.generic.size);
        }
        else 
        {
            std::fprintf(stderr, "[PUB] Failed to publish generic frame (err=%d)\n", rc);
        }

        cycle++;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    /* Destroy the hsil session. */
    hsil_destroy(handle);

    return EXIT_SUCCESS;
}
