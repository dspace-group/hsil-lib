/** <!-------------------------------------------------------------------------->
*
*   @file GroupedDataSub.cpp
*
*   @brief HSIL CoSim GroupedData Subscriber example (without hsil config)
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

struct SensorPayload {
    float temp;
    float humidity;
    uint32_t counter;
};

int deserializeSensorPayload(const uint8_t* buffer, size_t bufferSize, SensorPayload& payload)
{
    if (bufferSize < sizeof(SensorPayload)) {
        return -1;
    }

    size_t offset = 0;

    /* copy temperature. */
    std::memcpy(&payload.temp, (buffer+offset), sizeof(payload.temp));
    offset += sizeof(payload.temp);

    /* copy humidity. */
    std::memcpy(&payload.humidity, (buffer+offset), sizeof(payload.humidity));
    offset += sizeof(payload.humidity);

    /* copy counter. */
    std::memcpy(&payload.counter, (buffer+offset), sizeof(payload.counter));
    offset += sizeof(payload.counter);

    return 0;
}

static void onGroupedData(const char* topicName, const HsilGroupedData* data, void* userData)
{
    (void)userData;
    if (data->dataSize < sizeof(SensorPayload)) {
        std::printf("[SUB] Received small payload for topic %s: %zu bytes (expected >= %zu)\n",
                    topicName, data->dataSize, sizeof(SensorPayload));
        return;
    }

    SensorPayload payload{};
    if (deserializeSensorPayload(data->data, data->dataSize, payload) != 0) {
        std::fprintf(stderr, "[SUB] Failed to deserialize payload for topic %s\n", topicName);
        return;
    }

    std::printf("[SUB] topic=%s id=%u seq=%u temp=%.2f humidity=%.2f counter=%u\n",
                topicName, data->id, data->sequenceNumber,
                payload.temp, payload.humidity, payload.counter);
}

int main()
{
    int domainId = 42;  // must match publisher's domain
    const char* topicName = "GroupedTopic"; // must match publisher's topic
    HsilQos qos = HSIL_DEFAULT_QOS; // Use default QoS; can be customized if desired

    std::printf("=== HSIL GroupedData Subscriber (without hsil config) ===\n");

    std::printf("Domain: %d\n", domainId);
    std::printf("Topic Name: %s\n", topicName);
    std::printf("QoS Priority: %d (0=normal, 1=low, 2=high)\n", qos.priority);
    std::printf("QoS Queue Length: %d\n", qos.queueLength);
    std::printf("QoS Reliability: %d (0=fast, 1=reliable)\n\n", qos.reliability);

    /* Create hsil session on the specified domain. */
    HsilHandle handle = nullptr;
    int rc = hsil_create(domainId, nullptr, &handle);
    if (rc != HSIL_OK) {
        std::fprintf(stderr, "Error: Failed to create HSIL session (err=%d).\n", rc);
        return EXIT_FAILURE;
    }

    /* Subscribe to the topic with HSIL_ALL_IDS so we get everything.
       Callback will be invoked for each received grouped data sample. */
    rc = hsil_subscribe_grouped(handle, topicName, &qos, HSIL_ALL_IDS, onGroupedData, nullptr);
    if (rc != HSIL_OK) {
        std::fprintf(stderr, "Error: Failed to subscribe to topic (err=%d).\n", rc);
        hsil_destroy(handle);
        return EXIT_FAILURE;
    }

    std::printf("Subscribed! Listening for messages. Press Ctrl+C to terminate.\n\n");

    /* Poll the subscriber status so publishers joining or leaving are visible.
       Without this, "no data" looks identical whether no publisher was ever
       discovered or one was rejected because of incompatible QoS. */
    int lastMatchedCount = -1;
    int lastQosMismatchCount = 0;
    while (true) {
        HsilEndpointStatus status{};
        if (hsil_get_subscriber_status(handle, topicName, &status) == HSIL_OK) {

            if (status.matchedCount != lastMatchedCount) {
                std::printf("[SUB] Matched publishers: %d\n", status.matchedCount);
                lastMatchedCount = status.matchedCount;
            }

            if (status.qosMismatchCount != lastQosMismatchCount) {
                std::fprintf(stderr,
                             "[SUB] Warning: %d publisher(s) rejected due to incompatible QoS%s\n",
                             status.qosMismatchCount,
                             status.lastMismatchedQosPolicy == HSIL_QOS_MISMATCH_RELIABILITY
                                 ? " (reliability: this subscriber requires RELIABLE, the publisher is FAST)"
                                 : "");
                lastQosMismatchCount = status.qosMismatchCount;
            }
        }

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    /* Destroy the hsil session. */
    hsil_destroy(handle);

    return EXIT_SUCCESS;
}
