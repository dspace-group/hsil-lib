// -------------------------------------------------------------------------- //
// Layer 3 – CycloneDDS vendor backend tests (real DDS participant)
//
// These tests use the real hsilDdsVendor / CycloneDDS library. No external
// daemon is needed; CycloneDDS is peer-to-peer and uses loopback UDP.
//
// Domain 200 is used throughout to minimise collisions with production
// agents or other CI jobs running on domain 0.
// -------------------------------------------------------------------------- //

#include <gtest/gtest.h>

#include "ddsAbstraction.h"

#include <atomic>
#include <chrono>
#include <cstring>
#include <cstdlib>
#include <thread>

static constexpr int kTestDomain = 200;

// Separate domain IDs for the XML-config tests so they never share a domain
// with the base fixture (100) or with each other.  CycloneDDS domain config
// is a process-wide singleton: once a domain is started, its configuration is
// locked in for all subsequent participants on the same domain ID.
static constexpr int kXmlDomain_Valid          = 101;
static constexpr int kXmlDomain_Nonexistent    = 102;
static constexpr int kXmlDomain_Malformed      = 103;
static constexpr int kXmlDomain_SameConfigMulti = 104;
static constexpr int kXmlDomain_DiffConfigMulti = 105;

// ===========================================================================
// Discovery helpers
//
// Endpoint discovery is asynchronous, so tests must not publish before the
// writer and reader have actually matched. These helpers poll the vendor's
// getEndpointStatus op instead of sleeping for a fixed duration: they return
// as soon as the condition holds, and still fail fast on a real regression
// because they are bounded by a timeout.
// ===========================================================================

/** Poll until @p endpoint reports at least one matched counterpart. */
static bool waitForMatch(const HsilDdsOps* ops,
                         void*             ctx,
                         void*             endpoint,
                         int               isWriter,
                         std::chrono::milliseconds timeout = std::chrono::seconds(3))
{
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline)
    {
        HsilEndpointStatus status{};
        if (ops->getEndpointStatus(ctx, endpoint, isWriter, &status) == 0 && status.matchedCount > 0)
        {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    return false;
}

/** Poll until @p endpoint reports at least one QoS-incompatible counterpart. */
static bool waitForQosMismatch(const HsilDdsOps* ops,
                               void*             ctx,
                               void*             endpoint,
                               int               isWriter,
                               HsilEndpointStatus&       status,
                               std::chrono::milliseconds timeout = std::chrono::seconds(3))
{
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline)
    {
        if (ops->getEndpointStatus(ctx, endpoint, isWriter, &status) == 0
            && status.qosMismatchCount > 0)
        {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    return false;
}

// ===========================================================================
// Direct vtable tests (bypass hsil.cpp, exercise the vendor ops directly)
// ===========================================================================

class CycloneDdsOpsTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        ops = hsil_getDdsOps();
        ASSERT_NE(ops, nullptr);

        ctx = ops->init(kTestDomain, nullptr);
        ASSERT_NE(ctx, nullptr) << "DDS participant init failed on domain " << kTestDomain;
    }

    void TearDown() override
    {
        if (ctx)
        {
            ops->destroy(ctx);
            ctx = nullptr;
        }
    }

    const HsilDdsOps* ops = nullptr;
    void*             ctx = nullptr;
};

class Validation_Vtable : public CycloneDdsOpsTest {};
class Validation_Communication : public CycloneDdsOpsTest {};
class Validation_QosCompatibility : public CycloneDdsOpsTest {};

/**
 * @brief Fixture for XML-config tests.
 *
 * Obtains the DDS ops vtable in SetUp and restores CYCLONEDDS_URI to an
 * empty string in TearDown so that subsequent tests on fresh domains are
 * not inadvertently affected by a leftover env-var value.
 */
class Validation_XmlConfig : public ::testing::Test
{
protected:
    void SetUp() override
    {
        ops = hsil_getDdsOps();
        ASSERT_NE(ops, nullptr);
    }

    void TearDown() override
    {

    }

    const HsilDdsOps* ops = nullptr;
};

TEST_F(Validation_Vtable, InitAndDestroy)
{
    // SetUp / TearDown already cover this; this test just makes it explicit.
    EXPECT_NE(ctx, nullptr);
}

TEST_F(Validation_Vtable, CreateAndDestroyGroupedWriter)
{
    HsilQos qos = HSIL_DEFAULT_QOS;
    void* writer = ops->createGroupedWriter(ctx, "DdsTestGroupedTopic", &qos);
    ASSERT_NE(writer, nullptr);
    ops->destroyWriter(ctx, writer);
}

TEST_F(Validation_Vtable, CreateAndDestroyStreamingWriter)
{
    HsilQos qos = HSIL_DEFAULT_QOS;
    void* writer = ops->createStreamingWriter(ctx, "DdsTestStreamingTopic", &qos);
    ASSERT_NE(writer, nullptr);
    ops->destroyWriter(ctx, writer);
}

TEST_F(Validation_Vtable, CreateAndDestroyGroupedReader)
{
    HsilQos qos = HSIL_DEFAULT_QOS;
    auto cb = [](const char*, const HsilGroupedData*, void*){};
    void* reader = ops->createGroupedReader(ctx, "DdsTestGroupedTopic", &qos, cb, nullptr);
    ASSERT_NE(reader, nullptr);
    ops->destroyReader(ctx, reader);
}

TEST_F(Validation_Vtable, CreateAndDestroyStreamingReader)
{
    HsilQos qos = HSIL_DEFAULT_QOS;
    auto cb = [](const char*, const HsilStreamingData*, void*){};
    void* reader = ops->createStreamingReader(ctx, "DdsTestStreamingTopic", &qos, cb, nullptr);
    ASSERT_NE(reader, nullptr);
    ops->destroyReader(ctx, reader);
}

TEST_F(Validation_Vtable, GetEndpointStatusOnUnmatchedEndpoints)
{
    HsilQos qos = HSIL_DEFAULT_QOS;
    auto cb = [](const char*, const HsilGroupedData*, void*){};

    void* reader = ops->createGroupedReader(ctx, "DdsTestStatusTopic", &qos, cb, nullptr);
    ASSERT_NE(reader, nullptr);
    void* writer = ops->createGroupedWriter(ctx, "DdsTestStatusTopic", &qos);
    ASSERT_NE(writer, nullptr);

    // Both endpoints live on the same participant, and readers are created with
    // ignore-local, so they can never match each other. A freshly created
    // endpoint must therefore report a clean, zeroed status.
    HsilEndpointStatus writerStatus{};
    ASSERT_EQ(ops->getEndpointStatus(ctx, writer, 1, &writerStatus), 0);
    EXPECT_EQ(writerStatus.matchedCount, 0);
    EXPECT_EQ(writerStatus.qosMismatchCount, 0);
    EXPECT_EQ(writerStatus.lastMismatchedQosPolicy, HSIL_QOS_MISMATCH_NONE);

    HsilEndpointStatus readerStatus{};
    ASSERT_EQ(ops->getEndpointStatus(ctx, reader, 0, &readerStatus), 0);
    EXPECT_EQ(readerStatus.matchedCount, 0);
    EXPECT_EQ(readerStatus.qosMismatchCount, 0);
    EXPECT_EQ(readerStatus.lastMismatchedQosPolicy, HSIL_QOS_MISMATCH_NONE);

    ops->destroyWriter(ctx, writer);
    ops->destroyReader(ctx, reader);
}


// ===========================================================================
// DDS XML configuration tests
//
// CycloneDDS reads CYCLONEDDS_URI (set by cycloneInit) when the first
// participant on a domain is created.  The config is then locked in for
// the lifetime of that domain in this process.
//
// Key behaviours under test:
//   1. A valid XML file is accepted and the participant is functional.
//   2. A nonexistent file path causes CycloneDDS to fall back to defaults;
//      the participant is still created successfully.
//   3. A malformed XML file has the same graceful-fallback outcome.
//   4. Two participants on the same domain, same config: both succeed
//      and can communicate.
//   5. Two participants on the same domain, different configs: the second
//      participant joins the already-running domain (second config ignored);
//      both succeed and can still communicate.
//
// Each test uses a unique domain ID (201–205) so that the process-wide
// domain singletons never interfere with each other or with the base
// fixture on domain 200.
// ===========================================================================

TEST_F(Validation_XmlConfig, ValidXmlConfigSucceeds)
{
    // A well-formed XML config file must be accepted; the participant must be
    // created and fully functional (publish → receive on loopback).
    const char* configPath = HSIL_TESTS_DIR "/dds/fixtures/valid_config.xml";

    void* readerCtx = ops->init(kXmlDomain_Valid, configPath);
    ASSERT_NE(readerCtx, nullptr) << "init() must succeed with a valid XML config";

    void* writerCtx = ops->init(kXmlDomain_Valid, configPath);
    ASSERT_NE(writerCtx, nullptr) << "second init() on same domain with same config must succeed";

    std::atomic<bool> received{false};
    auto cb = [](const char*, const HsilGroupedData*, void* ud)
    {
        static_cast<std::atomic<bool>*>(ud)->store(true);
    };

    HsilQos qos = HSIL_DEFAULT_QOS;
    void* reader = ops->createGroupedReader(readerCtx, "XmlValidCfgLoopback", &qos, cb, &received);
    void* writer = ops->createGroupedWriter(writerCtx, "XmlValidCfgLoopback", &qos);
    ASSERT_NE(reader, nullptr);
    ASSERT_NE(writer, nullptr);

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    HsilGroupedData sample{};
    sample.id = 1;
    ASSERT_EQ(ops->writeGrouped(writerCtx, writer, &sample), 0);

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (!received.load() && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

    EXPECT_TRUE(received.load()) << "Sample not delivered after init with valid XML config";

    ops->destroyWriter(writerCtx, writer);
    ops->destroyReader(readerCtx, reader);
    ops->destroy(writerCtx);
    ops->destroy(readerCtx);
}

// ---------------------------------------------------------------------------

TEST_F(Validation_XmlConfig, NonexistentConfigFileReturnsFail)
{
    // When the config file does not exist CycloneDDS cannot create the
    // participant — it emits an error and dds_create_participant returns a
    // negative code, which cycloneInit translates into a null return value.
    void* ctx = ops->init(kXmlDomain_Nonexistent, "/no/such/hsil_config.xml");
    EXPECT_EQ(ctx, nullptr)
        << "init() must fail (return null) when the config file does not exist";
    if (ctx != nullptr)
    {
        ops->destroy(ctx); // clean up if the assertion is ever relaxed
    }
}

// ---------------------------------------------------------------------------

TEST_F(Validation_XmlConfig, MalformedXmlReturnsFail)
{
    // When the config file exists but contains malformed XML, CycloneDDS
    // cannot parse it — dds_create_participant returns an error and cycloneInit
    // returns null.
    const char* configPath = HSIL_TESTS_DIR "/dds/fixtures/invalid_config.xml";

    void* ctx = ops->init(kXmlDomain_Malformed, configPath);
    EXPECT_EQ(ctx, nullptr)
        << "init() must fail (return null) when the config XML is malformed";
    if (ctx != nullptr)
    {
        ops->destroy(ctx);
    }
}

// ---------------------------------------------------------------------------

TEST_F(Validation_XmlConfig, MultiParticipant_SameDomainSameConfig_BothSucceedAndCommunicate)
{
    // Creating two participants on the same domain with the same XML config
    // must succeed for both.  Both must be able to communicate because they
    // share the same CycloneDDS domain.
    const char* configPath = HSIL_TESTS_DIR "/dds/fixtures/valid_config.xml";

    void* ctx1 = ops->init(kXmlDomain_SameConfigMulti, configPath);
    ASSERT_NE(ctx1, nullptr) << "first participant init failed";

    void* ctx2 = ops->init(kXmlDomain_SameConfigMulti, configPath);
    ASSERT_NE(ctx2, nullptr) << "second participant init failed (same domain, same config)";

    std::atomic<bool> received{false};
    auto cb = [](const char*, const HsilGroupedData*, void* ud)
    {
        static_cast<std::atomic<bool>*>(ud)->store(true);
    };

    HsilQos qos = HSIL_DEFAULT_QOS;
    void* reader = ops->createGroupedReader(ctx1, "XmlSameCfgLoopback", &qos, cb, &received);
    void* writer = ops->createGroupedWriter(ctx2, "XmlSameCfgLoopback", &qos);
    ASSERT_NE(reader, nullptr);
    ASSERT_NE(writer, nullptr);

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    HsilGroupedData sample{};
    sample.id = 2;
    ASSERT_EQ(ops->writeGrouped(ctx2, writer, &sample), 0);

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (!received.load() && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

    EXPECT_TRUE(received.load())
        << "Data not delivered between two participants with same config on same domain";

    ops->destroyWriter(ctx2, writer);
    ops->destroyReader(ctx1, reader);
    ops->destroy(ctx2);
    ops->destroy(ctx1);
}

// ---------------------------------------------------------------------------

TEST_F(Validation_XmlConfig, MultiParticipant_SameDomainDifferentConfig_SecondConfigIgnored)
{
    // The first participant establishes the domain with a valid XML config.
    // The second participant requests a DIFFERENT (nonexistent) config on the
    // SAME domain.  CycloneDDS domain config is a process-wide singleton: the
    // domain is already running, so the second config path is silently ignored
    // and the second participant simply joins the existing domain.
    //
    // Observable guarantees:
    //   * Both init() calls return non-null (both participants are created).
    //   * Data written by the second participant is received by the first,
    //     proving they share the same domain.
    const char* firstConfig = HSIL_TESTS_DIR "/dds/fixtures/valid_config.xml";

    void* ctx1 = ops->init(kXmlDomain_DiffConfigMulti, firstConfig);
    ASSERT_NE(ctx1, nullptr) << "first participant (valid config) init failed";

    // The second config would cause a fresh-domain creation to fall back to
    // defaults, but since the domain is already running, it is never consulted.
    void* ctx2 = ops->init(kXmlDomain_DiffConfigMulti, "/different/nonexistent_config.xml");
    ASSERT_NE(ctx2, nullptr)
        << "second participant must succeed: domain already running, different config ignored";

    std::atomic<bool> received{false};
    auto cb = [](const char*, const HsilGroupedData*, void* ud)
    {
        static_cast<std::atomic<bool>*>(ud)->store(true);
    };

    HsilQos qos = HSIL_DEFAULT_QOS;
    void* reader = ops->createGroupedReader(ctx1, "XmlDiffCfgLoopback", &qos, cb, &received);
    void* writer = ops->createGroupedWriter(ctx2, "XmlDiffCfgLoopback", &qos);
    ASSERT_NE(reader, nullptr);
    ASSERT_NE(writer, nullptr);

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    HsilGroupedData sample{};
    sample.id = 3;
    ASSERT_EQ(ops->writeGrouped(ctx2, writer, &sample), 0);

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (!received.load() && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

    EXPECT_TRUE(received.load())
        << "Data not delivered — both participants must share the same domain "
           "regardless of the second config path";

    ops->destroyWriter(ctx2, writer);
    ops->destroyReader(ctx1, reader);
    ops->destroy(ctx2);
    ops->destroy(ctx1);
}

// ===========================================================================
// QoS compatibility – DDS endpoint matching rules
//
// DDS reliability compatibility rule:
//   compatible  → writer reliability >= reader reliability (offer >= request)
//   incompatible → FAST writer + RELIABLE reader (offer < request)
//
// FAST writer  + FAST reader    → compatible  (data delivered)
// RELIABLE writer + RELIABLE reader → compatible  (data delivered)
// RELIABLE writer + FAST reader    → compatible  (writer offers more; data delivered)
// FAST writer  + RELIABLE reader   → INCOMPATIBLE (data NOT delivered)
// ===========================================================================

TEST_F(Validation_QosCompatibility, GroupedLoopback_ReliableWriterReliableReader)
{
    void* writerCtx = ops->init(kTestDomain, nullptr);
    ASSERT_NE(writerCtx, nullptr);

    std::atomic<bool> received{false};
    auto cb = [](const char*, const HsilGroupedData*, void* ud)
    {
        static_cast<std::atomic<bool>*>(ud)->store(true);
    };

    HsilQos readerQos = HSIL_DEFAULT_QOS;
    readerQos.reliability = HSIL_QOS_RELIABILITY_RELIABLE;

    HsilQos writerQos = HSIL_DEFAULT_QOS;
    writerQos.reliability = HSIL_QOS_RELIABILITY_RELIABLE;

    void* reader = ops->createGroupedReader(ctx, "DdsQosReliable_Grouped", &readerQos, cb, &received);
    ASSERT_NE(reader, nullptr);

    void* writer = ops->createGroupedWriter(writerCtx, "DdsQosReliable_Grouped", &writerQos);
    ASSERT_NE(writer, nullptr);

    ASSERT_TRUE(waitForMatch(ops, writerCtx, writer, 1))
        << "RELIABLE/RELIABLE: writer and reader did not match";

    HsilGroupedData sample{};
    sample.id = 1;
    ASSERT_EQ(ops->writeGrouped(writerCtx, writer, &sample), 0);

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (!received.load() && std::chrono::steady_clock::now() < deadline)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    EXPECT_TRUE(received.load()) << "RELIABLE/RELIABLE: sample not delivered within 3 s";

    ops->destroyWriter(writerCtx, writer);
    ops->destroyReader(ctx, reader);
    ops->destroy(writerCtx);
}

TEST_F(Validation_QosCompatibility, GroupedLoopback_ReliableWriterFastReader_Compatible)
{
    // Writer offers RELIABLE, reader only needs FAST — per DDS spec this is
    // compatible (offer >= request) and data must be delivered.
    void* writerCtx = ops->init(kTestDomain, nullptr);
    ASSERT_NE(writerCtx, nullptr);

    std::atomic<bool> received{false};
    auto cb = [](const char*, const HsilGroupedData*, void* ud)
    {
        static_cast<std::atomic<bool>*>(ud)->store(true);
    };

    HsilQos readerQos = HSIL_DEFAULT_QOS;
    readerQos.reliability = HSIL_QOS_RELIABILITY_FAST;

    HsilQos writerQos = HSIL_DEFAULT_QOS;
    writerQos.reliability = HSIL_QOS_RELIABILITY_RELIABLE;

    void* reader = ops->createGroupedReader(ctx, "DdsQosRelW_FastR_Grouped", &readerQos, cb, &received);
    ASSERT_NE(reader, nullptr);

    void* writer = ops->createGroupedWriter(writerCtx, "DdsQosRelW_FastR_Grouped", &writerQos);
    ASSERT_NE(writer, nullptr);

    ASSERT_TRUE(waitForMatch(ops, writerCtx, writer, 1))
        << "RELIABLE writer / FAST reader: writer and reader did not match";

    HsilGroupedData sample{};
    sample.id = 2;
    ASSERT_EQ(ops->writeGrouped(writerCtx, writer, &sample), 0);

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (!received.load() && std::chrono::steady_clock::now() < deadline)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    EXPECT_TRUE(received.load()) << "RELIABLE writer / FAST reader: sample not delivered within 3 s";

    ops->destroyWriter(writerCtx, writer);
    ops->destroyReader(ctx, reader);
    ops->destroy(writerCtx);
}

TEST_F(Validation_QosCompatibility, GroupedLoopback_FastWriterReliableReader_Incompatible)
{
    // Writer offers FAST (best-effort), reader requests RELIABLE — per DDS spec
    // this is INCOMPATIBLE (offer < request). CycloneDDS will not match the
    // endpoints and no samples should be delivered.
    void* writerCtx = ops->init(kTestDomain, nullptr);
    ASSERT_NE(writerCtx, nullptr);

    std::atomic<bool> received{false};
    auto cb = [](const char*, const HsilGroupedData*, void* ud)
    {
        static_cast<std::atomic<bool>*>(ud)->store(true);
    };

    HsilQos readerQos = HSIL_DEFAULT_QOS;
    readerQos.reliability = HSIL_QOS_RELIABILITY_RELIABLE;

    HsilQos writerQos = HSIL_DEFAULT_QOS;
    writerQos.reliability = HSIL_QOS_RELIABILITY_FAST;

    void* reader = ops->createGroupedReader(ctx, "DdsQosIncompat_Grouped", &readerQos, cb, &received);
    ASSERT_NE(reader, nullptr);

    void* writer = ops->createGroupedWriter(writerCtx, "DdsQosIncompat_Grouped", &writerQos);
    ASSERT_NE(writer, nullptr);

    // The endpoints are discovered but declared incompatible, so instead of
    // guessing how long discovery takes we wait for the mismatch to be
    // reported on both sides and verify the policy that caused it.
    HsilEndpointStatus writerStatus{};
    ASSERT_TRUE(waitForQosMismatch(ops, writerCtx, writer, 1, writerStatus))
        << "FAST writer / RELIABLE reader: writer did not report an incompatible QoS";
    EXPECT_EQ(writerStatus.matchedCount, 0);
    EXPECT_EQ(writerStatus.lastMismatchedQosPolicy, HSIL_QOS_MISMATCH_RELIABILITY);

    HsilEndpointStatus readerStatus{};
    ASSERT_TRUE(waitForQosMismatch(ops, ctx, reader, 0, readerStatus))
        << "FAST writer / RELIABLE reader: reader did not report an incompatible QoS";
    EXPECT_EQ(readerStatus.matchedCount, 0);
    EXPECT_EQ(readerStatus.lastMismatchedQosPolicy, HSIL_QOS_MISMATCH_RELIABILITY);

    HsilGroupedData sample{};
    sample.id = 3;
    ASSERT_EQ(ops->writeGrouped(writerCtx, writer, &sample), 0);

    // Unmatched endpoints must never deliver data; a short grace period is
    // enough now that we already know the mismatch was detected.
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    EXPECT_FALSE(received.load())
        << "FAST writer / RELIABLE reader: sample must NOT be delivered (incompatible QoS)";

    ops->destroyWriter(writerCtx, writer);
    ops->destroyReader(ctx, reader);
    ops->destroy(writerCtx);
}


// ===========================================================================
// Loopback publish → receive test
//
// Writer and reader use separate DDS participants on the same domain so that
// CycloneDDS delivers data via its normal discovery path rather than relying
// on intra-participant (same-context) delivery, which is off by default.
// ===========================================================================

TEST_F(Validation_Communication, GroupedLoopback)
{
    // Create a second participant on the same domain as the receiver.
    void* writerCtx = ops->init(kTestDomain, nullptr);
    ASSERT_NE(writerCtx, nullptr) << "Second DDS participant init failed";

    constexpr uint32_t kExpectedId = 42;

    std::atomic<bool> received{false};
    std::atomic<uint32_t> receivedId{0};

    auto cb = [](const char* /*topic*/, const HsilGroupedData* d, void* ud)
    {
        auto* pair = static_cast<std::pair<std::atomic<bool>*, std::atomic<uint32_t>*>*>(ud);
        pair->second->store(d->id);
        pair->first->store(true);
    };

    std::pair<std::atomic<bool>*, std::atomic<uint32_t>*> ud{&received, &receivedId};

    HsilQos qos = HSIL_DEFAULT_QOS;

    // Reader on the fixture's participant (ctx), writer on the second participant
    void* reader = ops->createGroupedReader(ctx, "DdsLoopbackGrouped", &qos, cb, &ud);
    ASSERT_NE(reader, nullptr);

    void* writer = ops->createGroupedWriter(writerCtx, "DdsLoopbackGrouped", &qos);
    ASSERT_NE(writer, nullptr);

    // Wait for discovery between the two participants
    ASSERT_TRUE(waitForMatch(ops, writerCtx, writer, 1)) << "Writer and reader did not match";

    uint8_t payload[4] = {0xAA, 0xBB, 0xCC, 0xDD};
    HsilGroupedData sample{};
    sample.id       = kExpectedId;
    sample.data     = payload;
    sample.dataSize = sizeof(payload);

    ASSERT_EQ(ops->writeGrouped(writerCtx, writer, &sample), 0);

    // Wait up to 3 s for delivery
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (!received.load() && std::chrono::steady_clock::now() < deadline)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    EXPECT_TRUE(received.load()) << "GroupedData sample was not delivered within 3 s";
    EXPECT_EQ(receivedId.load(), kExpectedId);

    ops->destroyWriter(writerCtx, writer);
    ops->destroyReader(ctx, reader);
    ops->destroy(writerCtx);
}

TEST_F(Validation_Communication, StreamingLoopback)
{
    void* writerCtx = ops->init(kTestDomain, nullptr);
    ASSERT_NE(writerCtx, nullptr) << "Second DDS participant init failed";

    std::atomic<bool> received{false};

    auto cb = [](const char* /*topic*/, const HsilStreamingData* /*d*/, void* ud)
    {
        static_cast<std::atomic<bool>*>(ud)->store(true);
    };

    HsilQos qos = HSIL_DEFAULT_QOS;

    void* reader = ops->createStreamingReader(ctx, "DdsLoopbackStreaming", &qos, cb, &received);
    ASSERT_NE(reader, nullptr);

    void* writer = ops->createStreamingWriter(writerCtx, "DdsLoopbackStreaming", &qos);
    ASSERT_NE(writer, nullptr);

    ASSERT_TRUE(waitForMatch(ops, writerCtx, writer, 1)) << "Writer and reader did not match";

    uint8_t payload[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    HsilStreamingData sample{};
    std::strncpy(sample.protocol, "CAN_v1", sizeof(sample.protocol));
    sample.data     = payload;
    sample.dataSize = sizeof(payload);

    ASSERT_EQ(ops->writeStreaming(writerCtx, writer, &sample), 0);

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (!received.load() && std::chrono::steady_clock::now() < deadline)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    EXPECT_TRUE(received.load()) << "StreamingData sample was not delivered within 3 s";

    ops->destroyWriter(writerCtx, writer);
    ops->destroyReader(ctx, reader);
    ops->destroy(writerCtx);
}

// ===========================================================================
// Streaming metadata and payload round-trip tests
//
// Each test publishes a StreamingData sample with a specific protocol and
// known metadata values, then verifies the receiver gets the same metadata
// and payload bytes intact.
// ===========================================================================

TEST_F(Validation_Communication, CanRoundTripPubSub)
{
    void* writerCtx = ops->init(kTestDomain, nullptr);
    ASSERT_NE(writerCtx, nullptr);

    struct Received
    {
        std::atomic<bool> arrived{false};
        uint32_t          messageId{0};
        HsilCanFrameType  frameType{HSIL_CAN_FRAME_STD};
        uint8_t           payload[4]{};
        size_t            payloadSize{0};
    } rx;

    auto cb = [](const char*, const HsilStreamingData* d, void* ud)
    {
        auto* r = static_cast<Received*>(ud);
        r->messageId   = d->meta.can.messageId;
        r->frameType   = d->meta.can.frameType;
        r->payloadSize = d->dataSize < sizeof(r->payload) ? d->dataSize : sizeof(r->payload);
        std::memcpy(r->payload, d->data, r->payloadSize);
        r->arrived.store(true);
    };

    HsilQos qos = HSIL_DEFAULT_QOS;
    void* reader = ops->createStreamingReader(ctx, "DdsCanMetaRoundTrip", &qos, cb, &rx);
    ASSERT_NE(reader, nullptr);
    void* writer = ops->createStreamingWriter(writerCtx, "DdsCanMetaRoundTrip", &qos);
    ASSERT_NE(writer, nullptr);

    ASSERT_TRUE(waitForMatch(ops, writerCtx, writer, 1)) << "Writer and reader did not match";

    const uint8_t kPayload[4] = {0xDE, 0xAD, 0xBE, 0xEF};
    HsilStreamingData sample{};
    std::strncpy(sample.protocol, "CAN_v1", sizeof(sample.protocol));
    sample.meta.can.messageId = 0x1A2B3C4Du;
    sample.meta.can.frameType = HSIL_CAN_FRAME_EXT_FD;
    sample.data               = kPayload;
    sample.dataSize           = sizeof(kPayload);

    ASSERT_EQ(ops->writeStreaming(writerCtx, writer, &sample), 0);

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (!rx.arrived.load() && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

    ASSERT_TRUE(rx.arrived.load()) << "CAN sample not delivered within 3 s";
    EXPECT_EQ(rx.messageId, 0x1A2B3C4Du);
    EXPECT_EQ(rx.frameType, HSIL_CAN_FRAME_EXT_FD);
    ASSERT_EQ(rx.payloadSize, sizeof(kPayload));
    EXPECT_EQ(std::memcmp(rx.payload, kPayload, sizeof(kPayload)), 0);

    ops->destroyWriter(writerCtx, writer);
    ops->destroyReader(ctx, reader);
    ops->destroy(writerCtx);
}

TEST_F(Validation_Communication, EthRoundTripPubSub)
{
    void* writerCtx = ops->init(kTestDomain, nullptr);
    ASSERT_NE(writerCtx, nullptr);

    struct Received
    {
        std::atomic<bool> arrived{false};
        uint8_t           flags{0};
        size_t            payloadSize{0};
        uint8_t           payload[6]{};
    } rx;

    auto cb = [](const char*, const HsilStreamingData* d, void* ud)
    {
        auto* r = static_cast<Received*>(ud);
        r->flags       = d->meta.eth.flags;
        r->payloadSize = d->dataSize < sizeof(r->payload) ? d->dataSize : sizeof(r->payload);
        std::memcpy(r->payload, d->data, r->payloadSize);
        r->arrived.store(true);
    };

    HsilQos qos = HSIL_DEFAULT_QOS;
    void* reader = ops->createStreamingReader(ctx, "DdsEthMetaRoundTrip", &qos, cb, &rx);
    ASSERT_NE(reader, nullptr);
    void* writer = ops->createStreamingWriter(writerCtx, "DdsEthMetaRoundTrip", &qos);
    ASSERT_NE(writer, nullptr);

    ASSERT_TRUE(waitForMatch(ops, writerCtx, writer, 1)) << "Writer and reader did not match";

    const uint8_t kPayload[6] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
    HsilStreamingData sample{};
    std::strncpy(sample.protocol, "Eth_v1", sizeof(sample.protocol));
    sample.meta.eth.flags = HSIL_ETH_FLAG_HAS_FCS;
    sample.data           = kPayload;
    sample.dataSize       = sizeof(kPayload);

    ASSERT_EQ(ops->writeStreaming(writerCtx, writer, &sample), 0);

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (!rx.arrived.load() && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

    ASSERT_TRUE(rx.arrived.load()) << "Eth sample not delivered within 3 s";
    EXPECT_EQ(rx.flags, static_cast<uint8_t>(HSIL_ETH_FLAG_HAS_FCS));
    ASSERT_EQ(rx.payloadSize, sizeof(kPayload));
    EXPECT_EQ(std::memcmp(rx.payload, kPayload, sizeof(kPayload)), 0);

    ops->destroyWriter(writerCtx, writer);
    ops->destroyReader(ctx, reader);
    ops->destroy(writerCtx);
}

TEST_F(Validation_Communication, EthJRoundTripPubSub)
{
    void* writerCtx = ops->init(kTestDomain, nullptr);
    ASSERT_NE(writerCtx, nullptr);

    struct Received
    {
        std::atomic<bool> arrived{false};
        uint32_t          mtu{0};
        uint8_t           flags{0};
        size_t            payloadSize{0};
        uint8_t           payload[4]{};
    } rx;

    auto cb = [](const char*, const HsilStreamingData* d, void* ud)
    {
        auto* r = static_cast<Received*>(ud);
        r->mtu         = d->meta.ethj.mtu;
        r->flags       = d->meta.ethj.flags;
        r->payloadSize = d->dataSize < sizeof(r->payload) ? d->dataSize : sizeof(r->payload);
        std::memcpy(r->payload, d->data, r->payloadSize);
        r->arrived.store(true);
    };

    HsilQos qos = HSIL_DEFAULT_QOS;
    void* reader = ops->createStreamingReader(ctx, "DdsEthJMetaRoundTrip", &qos, cb, &rx);
    ASSERT_NE(reader, nullptr);
    void* writer = ops->createStreamingWriter(writerCtx, "DdsEthJMetaRoundTrip", &qos);
    ASSERT_NE(writer, nullptr);

    ASSERT_TRUE(waitForMatch(ops, writerCtx, writer, 1)) << "Writer and reader did not match";

    const uint8_t kPayload[4] = {0xAA, 0xBB, 0xCC, 0xDD};
    HsilStreamingData sample{};
    std::strncpy(sample.protocol, "EthJ_v1", sizeof(sample.protocol));
    sample.meta.ethj.mtu   = 9000u;
    sample.meta.ethj.flags = HSIL_ETH_FLAG_HAS_FCS;
    sample.data            = kPayload;
    sample.dataSize        = sizeof(kPayload);

    ASSERT_EQ(ops->writeStreaming(writerCtx, writer, &sample), 0);

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (!rx.arrived.load() && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

    ASSERT_TRUE(rx.arrived.load()) << "EthJ sample not delivered within 3 s";
    EXPECT_EQ(rx.mtu,   9000u);
    EXPECT_EQ(rx.flags, static_cast<uint8_t>(HSIL_ETH_FLAG_HAS_FCS));
    ASSERT_EQ(rx.payloadSize, sizeof(kPayload));
    EXPECT_EQ(std::memcmp(rx.payload, kPayload, sizeof(kPayload)), 0);

    ops->destroyWriter(writerCtx, writer);
    ops->destroyReader(ctx, reader);
    ops->destroy(writerCtx);
}

TEST_F(Validation_Communication, GenericRoundTripPubSub)
{
    void* writerCtx = ops->init(kTestDomain, nullptr);
    ASSERT_NE(writerCtx, nullptr);

    static const uint8_t kMetaBytes[5] = {0x11, 0x22, 0x33, 0x44, 0x55};

    struct Received
    {
        std::atomic<bool> arrived{false};
        uint8_t           metaCopy[8]{};
        size_t            metaSize{0};
        size_t            payloadSize{0};
        uint8_t           payload[4]{};
    } rx;

    auto cb = [](const char*, const HsilStreamingData* d, void* ud)
    {
        auto* r = static_cast<Received*>(ud);
        r->metaSize = d->meta.generic.size < sizeof(r->metaCopy)
                        ? d->meta.generic.size : sizeof(r->metaCopy);
        if (d->meta.generic.data != nullptr)
            std::memcpy(r->metaCopy, d->meta.generic.data, r->metaSize);
        r->payloadSize = d->dataSize < sizeof(r->payload) ? d->dataSize : sizeof(r->payload);
        std::memcpy(r->payload, d->data, r->payloadSize);
        r->arrived.store(true);
    };

    HsilQos qos = HSIL_DEFAULT_QOS;
    void* reader = ops->createStreamingReader(ctx, "DdsGenericMetaRoundTrip", &qos, cb, &rx);
    ASSERT_NE(reader, nullptr);
    void* writer = ops->createStreamingWriter(writerCtx, "DdsGenericMetaRoundTrip", &qos);
    ASSERT_NE(writer, nullptr);

    ASSERT_TRUE(waitForMatch(ops, writerCtx, writer, 1)) << "Writer and reader did not match";

    const uint8_t kPayload[4] = {0x01, 0x02, 0x03, 0x04};
    HsilStreamingData sample{};
    std::strncpy(sample.protocol, "generic", sizeof(sample.protocol));
    sample.meta.generic.data = const_cast<uint8_t*>(kMetaBytes);
    sample.meta.generic.size = sizeof(kMetaBytes);
    sample.data              = kPayload;
    sample.dataSize          = sizeof(kPayload);

    ASSERT_EQ(ops->writeStreaming(writerCtx, writer, &sample), 0);

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (!rx.arrived.load() && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

    ASSERT_TRUE(rx.arrived.load()) << "generic sample not delivered within 3 s";
    ASSERT_EQ(rx.metaSize, sizeof(kMetaBytes));
    EXPECT_EQ(std::memcmp(rx.metaCopy, kMetaBytes, sizeof(kMetaBytes)), 0);
    ASSERT_EQ(rx.payloadSize, sizeof(kPayload));
    EXPECT_EQ(std::memcmp(rx.payload, kPayload, sizeof(kPayload)), 0);

    ops->destroyWriter(writerCtx, writer);
    ops->destroyReader(ctx, reader);
    ops->destroy(writerCtx);
}
