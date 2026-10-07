// SPDX-FileCopyrightText: 2026 dSPACE SE & Co. KG
// SPDX-License-Identifier: Apache-2.0

/** <!-------------------------------------------------------------------------->
*
*   @file bmsEcu.cpp
*
*   @brief Battery Management System (BMS) ECU simulation.
*
*   @author
*       dSPACE SE & Co. KG
*
*   @description
*       Battery Management System (BMS) ECU simulation.
*       Subscribes to `BatterySignals` (grouped) and maintains a small local state
*       with the most recent cell temperatures. Runs a periodic control loop that
*       publishes a CAN command on `BmsCan` when any cell exceeds a threshold.
*
*   <hr><br>
*<!-------------------------------------------------------------------------->*/

#include <hsil/hsil.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <thread>
#include <vector>

using namespace std::chrono_literals;

struct BatteryTemperatures
{
    float temps[4];
};

static BatteryTemperatures g_batteryTemperatures;
static std::mutex g_batteryTemperaturesMutex;
static std::atomic<bool> g_running{true};

static uint64_t nowNs()
{
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
}

static void onBatteryPacket(const char* topicName, const HsilGroupedData* data, void* userData)
{
    (void)topicName; (void)userData;
    BatteryTemperatures temp{};
    HsilSignalValue val{};
    HsilSignalType t{};

    /* Read the cell temperatures from the grouped data. */
    for (size_t i = 0; i < 4; ++i)
    {
        if (hsil_read_signal_by_name(static_cast<HsilHandle>(userData), "BatterySignals", data, "battery.cell_temps", i, &val, &t) == HSIL_OK)
        {
            temp.temps[i] = val.asFloat32;
        }
        else
        {
            temp.temps[i] = 0.0f;
        }
    }

    /* Update the global state with the new temperatures. */
    {
        std::lock_guard<std::mutex> lk(g_batteryTemperaturesMutex);
        g_batteryTemperatures = temp;
    }
    
}

int main(int argc, char* argv[])
{
    const char* config = (argc > 1) ? argv[1] : "bmsConfig.json";
    std::printf("Starting BMS ECU, config=%s\n", config);

    /* Config mode: Create HSIL session from config */
    HsilHandle handle = nullptr;
    if (hsil_create_from_config(config, nullptr, &handle) != HSIL_OK)
    {
        std::fprintf(stderr, "Failed to create HSIL session from config.\n");
        return EXIT_FAILURE;
    }

    /* Subscribe to BatterySignals Topic to get updates on cell temperatures in onBatteryPacket callback */
    int rc = hsil_subscribe_grouped(handle, "BatterySignals", nullptr, HSIL_ALL_IDS, onBatteryPacket, handle);
    if (rc != HSIL_OK)
    {
        std::fprintf(stderr, "Failed to subscribe to BatterySignals (err=%d)\n", rc);
        hsil_destroy(handle);
        return EXIT_FAILURE;
    }

    const float kThreshold = 55.0f;

    /* Work loop */
    while (g_running.load())
    {
        /* Read the current battery temperatures. */
        BatteryTemperatures s;
        {
            std::lock_guard<std::mutex> lk(g_batteryTemperaturesMutex);
            s = g_batteryTemperatures;
        }

        /* Check if any cell exceeds the threshold. */
        bool over = false;
        for (float v : s.temps) if (v > kThreshold) { over = true; break; }

        /* Send the overtemperature status over CAN. */
        uint8_t payload[1];
        payload[0] = over ? 1 : 0;

        HsilStreamingData frame{};
        std::memcpy(frame.protocol, "CAN_v1", 6);
        frame.protocol[6] = '\0'; frame.protocol[7] = '\0';
        frame.timestamp = nowNs();
        frame.meta.can.messageId = 0x200u;
        frame.meta.can.frameType = HSIL_CAN_FRAME_STD;
        frame.data = payload;
        frame.dataSize = sizeof(payload);

        /* Publish the CAN frame containing overtemperature status on the BmsCan topic. */
        hsil_publish_streaming(handle, "BmsCan", &frame);

        std::printf("[BMS ECU] Cell temps: %.1f, %.1f, %.1f, %.1f | Over temperature detected: %s\n",
                    s.temps[0], s.temps[1], s.temps[2], s.temps[3], payload[0] ? "YES" : "NO");

        std::this_thread::sleep_for(10ms);
    }

    hsil_destroy(handle);
    return EXIT_SUCCESS;
}
