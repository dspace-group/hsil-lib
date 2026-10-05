#pragma once

// Shared header included by stub_dds_ops.cpp and test_hsilApi.cpp.
// Declares the global configuration struct that controls stub behaviour.

#include <hsil/hsil.h>

struct StubDdsCfg
{
    // --- Fail mode flags ---
    bool initShouldFail          = false;
    bool createWriterShouldFail  = false;
    bool createReaderShouldFail  = false;
    bool writeShouldFail         = false;
    bool getStatusShouldFail     = false;
    bool shouldThrow             = false; // When true, stub functions throw std::runtime_error

    // --- Call counters ---
    int initCallCount                  = 0;
    int destroyCallCount               = 0;
    int createGroupedWriterCallCount   = 0;
    int createStreamingWriterCallCount = 0;
    int destroyWriterCallCount         = 0;
    int writeGroupedCallCount          = 0;
    int writeStreamingCallCount        = 0;
    int createGroupedReaderCallCount   = 0;
    int createStreamingReaderCallCount = 0;
    int destroyReaderCallCount         = 0;
    int getEndpointStatusCallCount     = 0;

    // --- Captured callbacks (so tests can fire them manually) ---
    HsilGroupedDataCallback     lastGroupedCallback     = nullptr;
    void*                       lastGroupedCallbackData = nullptr;
    HsilStreamingDataCallback   lastStreamingCallback     = nullptr;
    void*                       lastStreamingCallbackData = nullptr;

    // --- Captured QoS (so tests can verify what was forwarded after sanitisation) ---
    HsilQos lastWriterQos = HSIL_DEFAULT_QOS;
    HsilQos lastReaderQos = HSIL_DEFAULT_QOS;

    // --- Endpoint status (so tests can verify what the API reports back) ---
    // statusToReturn is copied out by stub_getEndpointStatus; lastGetStatusIsWriter
    // records whether the API asked for the writer (1) or the reader (0) endpoint.
    HsilEndpointStatus statusToReturn        = { 0, 0, HSIL_QOS_MISMATCH_NONE };
    int        lastGetStatusIsWriter = -1;

    void reset()
    {
        *this = StubDdsCfg{};
    }
};

// Defined in stub_dds_ops.cpp; accessible from test code.
extern StubDdsCfg g_stubCfg;
