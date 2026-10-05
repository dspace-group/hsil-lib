/** <!-------------------------------------------------------------------------->
*
*   @file tempSensor.cpp
*
*   @brief Temperature sensor simulation.
*
*   @author
*       dSPACE SE & Co. KG
*
*   @description
*       Temperature sensor simulation that publishes a 4-element float32 array 
*       (battery.cell_temps) every 10 ms.
*       Interactive: type "o" (over) or "n" (normal) on stdin to change values.
*
*   @copyright
*       Copyright 2026, dSPACE SE & Co. KG. All rights reserved.
*
*   <hr><br>
*<!-------------------------------------------------------------------------->*/

#include <hsil/hsil.h>

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

using namespace std::chrono_literals;

static std::atomic<bool> g_running{true};
static std::atomic<bool> g_overtemp{false};

static void sigint_handler(int)
{
    g_running.store(false);
}

static void input_thread()
{
    std::string line;

    std::printf("Input thread started. Type 'o' or 'over' to simulate overtemperature, 'n' or 'normal' for normal temperatures. Ctrl-C to exit.\n");

    while (g_running.load())
    {
        if (!std::getline(std::cin, line)) break;
        if (line.empty()) continue;
        if ((line[0] == 'o') || (line == "over"))
            g_overtemp.store(true);
        else if ((line[0] == 'n') || (line == "normal"))
            g_overtemp.store(false);
    }
    g_running.store(false);
}

static uint64_t nowNs()
{
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
}

int main(int argc, char* argv[])
{
    std::signal(SIGINT, sigint_handler);

    const char* config = (argc > 1) ? argv[1] : "sensorConfig.json";
    std::printf("Starting temp sensor, config=%s\n", config);

    /* Config mode: Create HSIL session from config. */
    HsilHandle handle = nullptr;
    if (hsil_create_from_config(config, nullptr, &handle) != HSIL_OK)
    {
        std::fprintf(stderr, "Failed to create HSIL session from config.\n");
        return EXIT_FAILURE;
    }

    std::thread inThread(input_thread);

    size_t payloadSize = 0;
    if (hsil_get_grouped_data_size(handle, 1u, &payloadSize) != HSIL_OK)
    {
        std::fprintf(stderr, "Failed to query grouped data size for group id 1.\n");
        hsil_destroy(handle);
        return EXIT_FAILURE;
    }
    std::vector<uint8_t> payload(payloadSize, 0u);

    HsilGroupedData pkt{};
    pkt.id = 1u; // matches config group id
    pkt.data = payload.data();
    pkt.dataSize = payloadSize;

    while (g_running.load())
    {
        float temps[4];
        if (g_overtemp.load())
        {
            temps[0] = 58.0f; // one cell overtemperature
            temps[1] = 56.5f;
            temps[2] = 54.0f;
            temps[3] = 53.2f;
        }
        else
        {
            temps[0] = 30.1f;
            temps[1] = 30.2f;
            temps[2] = 30.0f;
            temps[3] = 29.8f;
        }

        /* Write signals element-by-element using the config-mode helper. */
        for (size_t i = 0; i < 4; ++i)
        {
            HsilSignalValue v{};
            v.asFloat32 = temps[i];
            int rc = hsil_write_signal_by_name(handle, "BatterySignals", &pkt, "battery.cell_temps", i, v, HSIL_SIGNAL_FLOAT32);
            if (rc != HSIL_OK && rc != HSIL_WARN_SIGNAL_CONVERSION)
            {
                std::fprintf(stderr, "Failed to write signal element %zu (err=%d)\n", i, rc);
            }
        }

        pkt.timestamp = nowNs();
        pkt.sequenceNumber = 0u;

        /* Publish the grouped data on the BatterySignals topic. */
        hsil_publish_grouped(handle, "BatterySignals", &pkt);

        std::this_thread::sleep_for(10ms);
    }

    if (inThread.joinable()) inThread.join();
    hsil_destroy(handle);
    return EXIT_SUCCESS;
}
