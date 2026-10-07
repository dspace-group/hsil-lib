// SPDX-FileCopyrightText: 2026 dSPACE SE & Co. KG
// SPDX-License-Identifier: Apache-2.0

/** <!-------------------------------------------------------------------------->
*
*   @file coolingEcu.cpp
*
*   @brief Cooling ECU simulation.
*
*   @author
*       dSPACE SE & Co. KG
*
*   @description
*       Cooling ECU simulation that subscribes to `BmsCan` topic and reacts to the 
*       cooling command (prints action to stdout).
*
*   <hr><br>
*<!-------------------------------------------------------------------------->*/

#include <hsil/hsil.h>

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>

static void onCan(const char* topicName, const HsilStreamingData* data, void* userData)
{
    (void)topicName; (void)userData;
    if (data->meta.can.messageId != 0x200u) return;
    if ((data->data == nullptr) || (data->dataSize < 1)) return;

    uint8_t cmd = data->data[0];
    if (cmd == 1)
    {
        std::printf("[Cooling ECU RX] Action: OVERTEMPERATURE detected! Starting cooling loop [POWER = 100%%]\n");
    }
    else
    {
        std::printf("[Cooling ECU RX] Action: Temperatures normal. Cooling loop is [STANDBY]\n");
    }
}

int main(int argc, char* argv[])
{
    const char* config = (argc > 1) ? argv[1] : "coolingConfig.json";
    std::printf("Starting Cooling ECU, config=%s\n", config);

    /* Config mode: Create HSIL session from config */
    HsilHandle handle = nullptr;
    if (hsil_create_from_config(config, nullptr, &handle) != HSIL_OK)
    {
        std::fprintf(stderr, "Failed to create HSIL session from config.\n");
        return EXIT_FAILURE;
    }

    /* Subscribe to BmsCan Topic to get updates on cooling commands in onCan callback. */
    int rc = hsil_subscribe_streaming(handle, "BmsCan", nullptr, "CAN_v1", onCan, nullptr);
    if (rc != HSIL_OK)
    {
        std::fprintf(stderr, "Failed to subscribe to BmsCan (err=%d)\n", rc);
        hsil_destroy(handle);
        return EXIT_FAILURE;
    }

    // Keep running until interrupted by user (Ctrl-C)
    for (;;)
        std::this_thread::sleep_for(std::chrono::seconds(1));

    hsil_destroy(handle);
    return EXIT_SUCCESS;
}
