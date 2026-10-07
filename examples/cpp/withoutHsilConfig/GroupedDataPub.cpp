// SPDX-FileCopyrightText: 2026 dSPACE SE & Co. KG
// SPDX-License-Identifier: Apache-2.0

/** <!-------------------------------------------------------------------------->
*
*   @file GroupedDataPub.cpp
*
*   @brief HSIL CoSim GroupedData Publisher example (without hsil config)
*
*   @author
*       dSPACE SE & Co. KG
*
*   @description
*       Creates a publisher and sends GroupedData samples on a configured topic
*       without using an HSIL JSON configuration file.
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

int serializeSensorPayload(const SensorPayload& payload, uint8_t* buffer, size_t bufferSize)
{
    if (bufferSize < sizeof(SensorPayload)) {
        return -1;
    }

    size_t offset = 0;

    /* copy temperature. */
    std::memcpy((buffer+offset), &payload.temp, sizeof(payload.temp));
    offset += sizeof(payload.temp);

    /* copy humidity. */
    std::memcpy((buffer+offset), &payload.humidity, sizeof(payload.humidity));
    offset += sizeof(payload.humidity);

    /* copy counter. */
    std::memcpy((buffer+offset), &payload.counter, sizeof(payload.counter));
    offset += sizeof(payload.counter);

    return 0;
}

static uint64_t nowNs()
{
    using namespace std::chrono;
    return static_cast<uint64_t>(
        duration_cast<nanoseconds>(steady_clock::now().time_since_epoch()).count());
}

/**
 * @brief Report a warning when the publisher was rejected due to incompatible QoS.
 *
 * A non-zero qosMismatchCount means a subscriber was found but can never
 * receive data from this publisher. The usual cause is a FAST (best-effort)
 * publisher paired with a RELIABLE subscriber.
 */
static void reportQosMismatch(const HsilEndpointStatus& status)
{
    if (status.qosMismatchCount == 0) {
        return;
    }

    std::fprintf(stderr,
                 "[PUB] Warning: %d subscriber(s) rejected due to incompatible QoS%s\n",
                 status.qosMismatchCount,
                 status.lastMismatchedQosPolicy == HSIL_QOS_MISMATCH_RELIABILITY
                     ? " (reliability: this publisher is FAST, the subscriber requires RELIABLE)"
                     : "");
}

/**
 * @brief Block until at least one subscriber has been discovered.
 *
 * Samples published before a subscriber is matched are simply discarded, so
 * waiting here avoids silently losing the first cycles. The API call never
 * blocks, so discovery is awaited by polling it.
 *
 * @return true as soon as a subscriber is matched, false on timeout or error.
 */
static bool waitForSubscriber(HsilHandle handle, const char* topicName, int timeoutSeconds)
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(timeoutSeconds);

    while (std::chrono::steady_clock::now() < deadline) {
        HsilEndpointStatus status{};
        const int rc = hsil_get_publisher_status(handle, topicName, &status);
        if (rc != HSIL_OK) {
            std::fprintf(stderr, "[PUB] Failed to read publisher status (err=%d).\n", rc);
            return false;
        }

        if (status.matchedCount > 0) {
            std::printf("[PUB] Matched %d subscriber(s).\n", status.matchedCount);
            return true;
        }

        reportQosMismatch(status);

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    return false;
}

int main()
{
    int domainId = 42;  // must match subscriber's domain
    const char* topicName = "GroupedTopic"; // must match subscriber's topic
    HsilQos qos = HSIL_DEFAULT_QOS; // Use default QoS; can be customized if desired

    std::printf("=== HSIL GroupedData Publisher (without hsil config) ===\n");

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

    /* Create grouped data publisher with the specified topic and QoS. */
    rc = hsil_create_grouped_publisher(handle, topicName, &qos);
    if (rc != HSIL_OK) {
        std::fprintf(stderr, "Error: Failed to create grouped publisher (err=%d).\n", rc);
        hsil_destroy(handle);
        return EXIT_FAILURE;
    }

    /* Wait until a subscriber is listening; samples sent before a subscriber is
     * matched would be discarded. Publishing starts anyway on timeout so the
     * example still demonstrates the publish loop. */
    std::printf("Waiting for a subscriber on '%s'...\n", topicName);
    if (!waitForSubscriber(handle, topicName, 10)) {
        std::printf("[PUB] No subscriber matched yet; publishing anyway.\n");
    }

    std::printf("Publishing signal data every 1 second. Press Ctrl+C to terminate.\n\n");

    uint32_t cycle = 0;
    int lastMatchedCount = -1;
    uint8_t serializedPayload[sizeof(SensorPayload)] = {0};
    while (true) {

        /* Update sensor data for this cycle. */
        SensorPayload payload;
        payload.temp = 20.0f + static_cast<float>(cycle) * 0.5f;
        payload.humidity = 50.0f + static_cast<float>(cycle % 10);
        payload.counter = cycle;
        if (serializeSensorPayload(payload, serializedPayload, sizeof(serializedPayload)) != 0) {
            std::fprintf(stderr, "[PUB] Failed to serialize payload for cycle %u\n", cycle);
            break;
        }

        /* Prepare the grouped data sample. */
        HsilGroupedData sample{};
        sample.id = 1; // Group ID
        sample.sequenceNumber = cycle;
        sample.timestamp = nowNs();
        sample.data = serializedPayload;
        sample.dataSize = sizeof(serializedPayload);

        /* Publish the grouped data sample. */
        rc = hsil_publish_grouped(handle, topicName, &sample);
        if (rc == HSIL_OK) 
        {
            std::printf("[PUB] cycle=%u temp=%.2f humidity=%.2f counter=%u\n",
                        cycle, payload.temp, payload.humidity, payload.counter);
        }
        else 
        {
            std::fprintf(stderr, "[PUB] Failed to publish cycle %u (err=%d)\n", cycle, rc);
        }

        /* Report whenever subscribers join or leave, so it is obvious when
         * published samples stop reaching anyone. */
        HsilEndpointStatus status{};
        if (hsil_get_publisher_status(handle, topicName, &status) == HSIL_OK) {
            if (status.matchedCount != lastMatchedCount) {
                std::printf("[PUB] Matched subscribers: %d\n", status.matchedCount);
                lastMatchedCount = status.matchedCount;
                reportQosMismatch(status);
            }
        }

        cycle++;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    /* Destroy the hsil session. */
    hsil_destroy(handle);

    return EXIT_SUCCESS;
}
