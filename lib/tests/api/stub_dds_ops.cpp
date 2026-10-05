// -------------------------------------------------------------------------- //
// Layer 2 – DDS stub (linker-seam)
//
// Provides a minimal hsil_getDdsOps() implementation that replaces the real
// CycloneDDS backend at link time. No DDS daemon or network is needed.
//
// Design:
//   - g_stubCfg controls behaviour (fail modes, counters).
//   - Reset it in each test's SetUp() via StubDdsCfg::reset().
// -------------------------------------------------------------------------- //

#include "ddsAbstraction.h"
#include "stub_dds_ops.h"

#include <cstring>
#include <stdexcept>

// ---------------------------------------------------------------------------
// Global configuration (written from tests, read from stub functions)
// ---------------------------------------------------------------------------

StubDdsCfg g_stubCfg;

// Non-null sentinels used as opaque handles by the stub.
// reinterpret_cast is not allowed in constexpr; use static storage instead.
static int s_dummyCtxStorage    = 1;
static int s_dummyWriterStorage = 2;
static int s_dummyReaderStorage = 3;
static void* const kDummyCtx    = &s_dummyCtxStorage;
static void* const kDummyWriter = &s_dummyWriterStorage;
static void* const kDummyReader = &s_dummyReaderStorage;

// ---------------------------------------------------------------------------
// Stub function implementations
// ---------------------------------------------------------------------------

static void* stub_init(int /*domainId*/, const char* /*ddsConfigPath*/)
{
    ++g_stubCfg.initCallCount;
    if (g_stubCfg.shouldThrow) { throw std::runtime_error("stub_init: injected exception"); }
    return g_stubCfg.initShouldFail ? nullptr : kDummyCtx;
}

static void stub_destroy(void* /*ctx*/)
{
    ++g_stubCfg.destroyCallCount;
    if (g_stubCfg.shouldThrow) { throw std::runtime_error("stub_destroy: injected exception"); }
}

static void* stub_createGroupedWriter(void* /*ctx*/, const char* /*topicName*/,
                                       const HsilQos* qos)
{
    ++g_stubCfg.createGroupedWriterCallCount;
    if (qos) { g_stubCfg.lastWriterQos = *qos; }
    if (g_stubCfg.shouldThrow) { throw std::runtime_error("stub_createGroupedWriter: injected exception"); }
    return g_stubCfg.createWriterShouldFail ? nullptr : kDummyWriter;
}

static void* stub_createStreamingWriter(void* /*ctx*/, const char* /*topicName*/,
                                         const HsilQos* qos)
{
    ++g_stubCfg.createStreamingWriterCallCount;
    if (qos) { g_stubCfg.lastWriterQos = *qos; }
    if (g_stubCfg.shouldThrow) { throw std::runtime_error("stub_createStreamingWriter: injected exception"); }
    return g_stubCfg.createWriterShouldFail ? nullptr : kDummyWriter;
}

static void stub_destroyWriter(void* /*ctx*/, void* /*writer*/)
{
    ++g_stubCfg.destroyWriterCallCount;
}

static int stub_writeGrouped(void* /*ctx*/, void* /*writer*/,
                              const HsilGroupedData* /*sample*/)
{
    ++g_stubCfg.writeGroupedCallCount;
    if (g_stubCfg.shouldThrow) { throw std::runtime_error("stub_writeGrouped: injected exception"); }
    return g_stubCfg.writeShouldFail ? -1 : 0;
}

static int stub_writeStreaming(void* /*ctx*/, void* /*writer*/,
                                const HsilStreamingData* /*sample*/)
{
    ++g_stubCfg.writeStreamingCallCount;
    if (g_stubCfg.shouldThrow) { throw std::runtime_error("stub_writeStreaming: injected exception"); }
    return g_stubCfg.writeShouldFail ? -1 : 0;
}

static void* stub_createGroupedReader(void* /*ctx*/, const char* /*topicName*/,
                                       const HsilQos* qos,
                                       HsilGroupedDataCallback callback,
                                       void* callbackData)
{
    ++g_stubCfg.createGroupedReaderCallCount;
    if (qos) { g_stubCfg.lastReaderQos = *qos; }
    g_stubCfg.lastGroupedCallback     = callback;
    g_stubCfg.lastGroupedCallbackData = callbackData;
    if (g_stubCfg.shouldThrow) { throw std::runtime_error("stub_createGroupedReader: injected exception"); }
    return g_stubCfg.createReaderShouldFail ? nullptr : kDummyReader;
}

static void* stub_createStreamingReader(void* /*ctx*/, const char* /*topicName*/,
                                         const HsilQos* qos,
                                         HsilStreamingDataCallback callback,
                                         void* callbackData)
{
    ++g_stubCfg.createStreamingReaderCallCount;
    if (qos) { g_stubCfg.lastReaderQos = *qos; }
    g_stubCfg.lastStreamingCallback     = callback;
    g_stubCfg.lastStreamingCallbackData = callbackData;
    if (g_stubCfg.shouldThrow) { throw std::runtime_error("stub_createStreamingReader: injected exception"); }
    return g_stubCfg.createReaderShouldFail ? nullptr : kDummyReader;
}

static void stub_destroyReader(void* /*ctx*/, void* /*reader*/)
{
    ++g_stubCfg.destroyReaderCallCount;
    if (g_stubCfg.shouldThrow) { throw std::runtime_error("stub_destroyReader: injected exception"); }
}

static int stub_getEndpointStatus(void* /*ctx*/, void* /*endpoint*/, int isWriter, HsilEndpointStatus* status)
{
    ++g_stubCfg.getEndpointStatusCallCount;
    g_stubCfg.lastGetStatusIsWriter = isWriter;
    if (g_stubCfg.shouldThrow) { throw std::runtime_error("stub_getEndpointStatus: injected exception"); }
    if (g_stubCfg.getStatusShouldFail) { return -1; }
    *status = g_stubCfg.statusToReturn;
    return 0;
}

// ---------------------------------------------------------------------------
// Vtable registration – replaces cycloneDdsProvider.cpp at link time
// ---------------------------------------------------------------------------

static const HsilDdsOps kStubOps = {
    stub_init,
    stub_destroy,
    stub_createGroupedWriter,
    stub_createStreamingWriter,
    stub_destroyWriter,
    stub_writeGrouped,
    stub_writeStreaming,
    stub_createGroupedReader,
    stub_createStreamingReader,
    stub_destroyReader,
    stub_getEndpointStatus,
};

const HsilDdsOps* hsil_getDdsOps(void)
{
    return &kStubOps;
}
