// SPDX-FileCopyrightText: 2026 dSPACE SE & Co. KG
// SPDX-License-Identifier: Apache-2.0

/** <!-------------------------------------------------------------------------->
*
*   @file test_hsilApi.cpp
*
*   @brief Layer 2 HSIL API tests using a stub DDS vtable.
*
*   @author
*       dSPACE SE & Co. KG
*
*   @description
*       Tests hsil.cpp argument validation, error propagation, subscriber
*       dispatch, publisher type enforcement, get-or-create semantics, and
*       topic type enforcement without requiring live DDS.
*
*   <hr><br>
*<!-------------------------------------------------------------------------->*/

#include <gtest/gtest.h>

#include <hsil/hsil.h>
#include "stub_dds_ops.h"

#include <atomic>
#include <cstring>
#include <string>

// ===========================================================================
// Test fixture – resets the stub before every test
// ===========================================================================

class HsilApiTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        g_stubCfg.reset();
    }

    void TearDown() override
    {
        // Nothing; each test is responsible for destroying handles it creates.
    }
};

// Derive from the base fixture to group related tests together
class Validation_hsil_create : public HsilApiTest {};
class Validation_hsil_destroy : public HsilApiTest {};
class Validation_hsil_subscribe_grouped : public HsilApiTest {};
class Validation_hsil_subscribe_streaming : public HsilApiTest {};
class Validation_TopicTypeMismatch : public HsilApiTest {};
class Validation_hsil_create_grouped_publisher : public HsilApiTest {};
class Validation_hsil_create_grouped_subscriber : public HsilApiTest {};
class Validation_hsil_create_streaming_publisher : public HsilApiTest {};
class Validation_hsil_create_streaming_subscriber : public HsilApiTest {};
class Validation_hsil_publish_grouped : public HsilApiTest {};
class Validation_hsil_publish_streaming : public HsilApiTest {};
class Validation_hsil_endpoint_get_status : public HsilApiTest {};
class Validation_QoS : public HsilApiTest {};
class Validation_hsil_check_signal_conversion : public HsilApiTest {};
class Validation_hsil_read_signal_by_name : public HsilApiTest {};
class Validation_hsil_write_signal_by_name : public HsilApiTest {};
class Validation_hsil_get_grouped_data_size : public HsilApiTest {};

// ===========================================================================
// hsil_create
// ===========================================================================

TEST_F(Validation_hsil_create, NullHandleReturnsInvalidArg)
{
    EXPECT_EQ(hsil_create(0, nullptr, nullptr), HSIL_ERR_INVALID_ARG);
    EXPECT_EQ(g_stubCfg.initCallCount, 0);  // must not touch DDS
}

TEST_F(Validation_hsil_create, Success)
{
    HsilHandle h = nullptr;
    ASSERT_EQ(hsil_create(0, nullptr, &h), HSIL_OK);
    EXPECT_NE(h, nullptr);
    EXPECT_EQ(g_stubCfg.initCallCount, 1);

    hsil_destroy(h);
    EXPECT_EQ(g_stubCfg.destroyCallCount, 1);
}

TEST_F(Validation_hsil_create, DdsInitFailureReturnsDdsError)
{
    g_stubCfg.initShouldFail = true;

    HsilHandle h = nullptr;
    EXPECT_EQ(hsil_create(0, nullptr, &h), HSIL_ERR_DDS);
    EXPECT_EQ(h, nullptr);
}

TEST_F(Validation_hsil_create, DoesNotThrow)
{
    g_stubCfg.shouldThrow = true;
    HsilHandle h = nullptr;
    EXPECT_NO_THROW({
        int rc = hsil_create(0, nullptr, &h);
        EXPECT_EQ(rc, HSIL_ERR_GENERIC);
    });
}

TEST_F(Validation_hsil_destroy, NullHandleIsNoop)
{
    // Must not crash
    hsil_destroy(nullptr);
    EXPECT_EQ(g_stubCfg.destroyCallCount, 0);
}

TEST_F(Validation_hsil_destroy, ShouldNotThrow)
{
    HsilHandle h = nullptr;
    ASSERT_EQ(hsil_create(0, nullptr, &h), HSIL_OK);
    g_stubCfg.shouldThrow = true;
    EXPECT_NO_THROW(hsil_destroy(h));
}

// ===========================================================================
// hsil_subscribe_grouped – get-or-create reader + callback dispatch
// ===========================================================================

TEST_F(Validation_hsil_subscribe_grouped, NullHandleReturnsInvalidArg)
{
    auto cb = [](const char*, const HsilGroupedData*, void*){};
    EXPECT_EQ(hsil_subscribe_grouped(nullptr, "T", nullptr, HSIL_ALL_IDS, cb, nullptr),
              HSIL_ERR_INVALID_ARG);
}

TEST_F(Validation_hsil_subscribe_grouped, NullTopicReturnsInvalidArg)
{
    HsilHandle h = nullptr;
    ASSERT_EQ(hsil_create(0, nullptr, &h), HSIL_OK);

    auto cb = [](const char*, const HsilGroupedData*, void*){};
    EXPECT_EQ(hsil_subscribe_grouped(h, nullptr, nullptr, HSIL_ALL_IDS, cb, nullptr),
              HSIL_ERR_INVALID_ARG);

    hsil_destroy(h);
}

TEST_F(Validation_hsil_subscribe_grouped, NullCallbackReturnsInvalidArg)
{
    HsilHandle h = nullptr;
    ASSERT_EQ(hsil_create(0, nullptr, &h), HSIL_OK);

    EXPECT_EQ(hsil_subscribe_grouped(h, "T", nullptr, HSIL_ALL_IDS, nullptr, nullptr),
              HSIL_ERR_INVALID_ARG);

    hsil_destroy(h);
}

TEST_F(Validation_hsil_subscribe_grouped, CreatesReaderOnFirstCall)
{
    HsilHandle h = nullptr;
    ASSERT_EQ(hsil_create(0, nullptr, &h), HSIL_OK);

    auto cb = [](const char*, const HsilGroupedData*, void*){};
    ASSERT_EQ(hsil_subscribe_grouped(h, "TopicA", nullptr, HSIL_ALL_IDS, cb, nullptr), HSIL_OK);
    EXPECT_EQ(g_stubCfg.createGroupedReaderCallCount, 1);

    // Second subscribe on the same topic must reuse the reader
    ASSERT_EQ(hsil_subscribe_grouped(h, "TopicA", nullptr, HSIL_ALL_IDS, cb, nullptr), HSIL_OK);
    EXPECT_EQ(g_stubCfg.createGroupedReaderCallCount, 1);

    hsil_destroy(h);
}

TEST_F(Validation_hsil_subscribe_grouped, ReaderFailureReturnsDdsError)
{
    g_stubCfg.createReaderShouldFail = true;

    HsilHandle h = nullptr;
    ASSERT_EQ(hsil_create(0, nullptr, &h), HSIL_OK);

    auto cb = [](const char*, const HsilGroupedData*, void*){};
    EXPECT_EQ(hsil_subscribe_grouped(h, "T", nullptr, HSIL_ALL_IDS, cb, nullptr), HSIL_ERR_DDS);

    hsil_destroy(h);
}

TEST_F(Validation_hsil_subscribe_grouped, CallbackReceivedOnMatch)
{
    HsilHandle h = nullptr;
    ASSERT_EQ(hsil_create(0, nullptr, &h), HSIL_OK);

    std::atomic<int> hitCount{0};
    auto cb = [](const char*, const HsilGroupedData* d, void* ud)
    {
        (void)d;
        ++(*static_cast<std::atomic<int>*>(ud));
    };

    ASSERT_EQ(hsil_subscribe_grouped(h, "T", nullptr, HSIL_ALL_IDS, cb, &hitCount), HSIL_OK);

    // Fire the dispatch callback directly (no DDS network needed)
    ASSERT_NE(g_stubCfg.lastGroupedCallback, nullptr);
    uint8_t payload[] = {1, 2, 3};
    HsilGroupedData sample{};
    sample.id       = 7;
    sample.data     = payload;
    sample.dataSize = sizeof(payload);

    g_stubCfg.lastGroupedCallback("T", &sample, g_stubCfg.lastGroupedCallbackData);

    EXPECT_EQ(hitCount.load(), 1);

    hsil_destroy(h);
}

TEST_F(Validation_hsil_subscribe_grouped, GroupIdFilterWorks)
{
    HsilHandle h = nullptr;
    ASSERT_EQ(hsil_create(0, nullptr, &h), HSIL_OK);

    std::atomic<int> hitCount{0};
    auto cb = [](const char*, const HsilGroupedData*, void* ud)
    {
        ++(*static_cast<std::atomic<int>*>(ud));
    };

    // Subscribe with groupId=5 — only samples with id==5 should trigger it
    ASSERT_EQ(hsil_subscribe_grouped(h, "T", nullptr, /*groupId=*/5, cb, &hitCount), HSIL_OK);

    HsilGroupedData mismatch{};
    mismatch.id = 99;  // different id — must be filtered out
    g_stubCfg.lastGroupedCallback("T", &mismatch, g_stubCfg.lastGroupedCallbackData);
    EXPECT_EQ(hitCount.load(), 0);

    HsilGroupedData match{};
    match.id = 5;  // matching id — must be delivered
    g_stubCfg.lastGroupedCallback("T", &match, g_stubCfg.lastGroupedCallbackData);
    EXPECT_EQ(hitCount.load(), 1);

    hsil_destroy(h);
}

TEST_F(Validation_hsil_subscribe_grouped, TwoCallbacksBothFire)
{
    HsilHandle h = nullptr;
    ASSERT_EQ(hsil_create(0, nullptr, &h), HSIL_OK);

    std::atomic<int> hitCount{0};
    auto cb = [](const char*, const HsilGroupedData*, void* ud)
    {
        ++(*static_cast<std::atomic<int>*>(ud));
    };

    // Two subscribe calls on the same topic — reader created once, both callbacks fire
    ASSERT_EQ(hsil_subscribe_grouped(h, "T", nullptr, HSIL_ALL_IDS, cb, &hitCount), HSIL_OK);
    ASSERT_EQ(hsil_subscribe_grouped(h, "T", nullptr, HSIL_ALL_IDS, cb, &hitCount), HSIL_OK);
    EXPECT_EQ(g_stubCfg.createGroupedReaderCallCount, 1);

    HsilGroupedData sample{};
    sample.id = 1;
    g_stubCfg.lastGroupedCallback("T", &sample, g_stubCfg.lastGroupedCallbackData);
    EXPECT_EQ(hitCount.load(), 2);  // both callbacks fired

    hsil_destroy(h);
}

TEST_F(Validation_hsil_subscribe_grouped, DoesNotThrow)
{
    HsilHandle h = nullptr;
    ASSERT_EQ(hsil_create(0, nullptr, &h), HSIL_OK);
    g_stubCfg.shouldThrow = true;

    auto cb = [](const char*, const HsilGroupedData*, void*) {};
    EXPECT_NO_THROW({
        int rc = hsil_subscribe_grouped(h, "T", nullptr, 1u, cb, nullptr);
        EXPECT_EQ(rc, HSIL_ERR_GENERIC);
    });
    g_stubCfg.shouldThrow = false;
    hsil_destroy(h);
}

// ===========================================================================
// hsil_subscribe_streaming – get-or-create reader + callback dispatch
// ===========================================================================

TEST_F(Validation_hsil_subscribe_streaming, NullHandleReturnsInvalidArg)
{
    auto cb = [](const char*, const HsilStreamingData*, void*){};
    EXPECT_EQ(hsil_subscribe_streaming(nullptr, "T", nullptr, nullptr, cb, nullptr),
              HSIL_ERR_INVALID_ARG);
}

TEST_F(Validation_hsil_subscribe_streaming, NullTopicReturnsInvalidArg)
{
    HsilHandle h = nullptr;
    ASSERT_EQ(hsil_create(0, nullptr, &h), HSIL_OK);

    auto cb = [](const char*, const HsilStreamingData*, void*){};
    EXPECT_EQ(hsil_subscribe_streaming(h, nullptr, nullptr, nullptr, cb, nullptr),
              HSIL_ERR_INVALID_ARG);

    hsil_destroy(h);
}

TEST_F(Validation_hsil_subscribe_streaming, NullCallbackReturnsInvalidArg)
{
    HsilHandle h = nullptr;
    ASSERT_EQ(hsil_create(0, nullptr, &h), HSIL_OK);

    EXPECT_EQ(hsil_subscribe_streaming(h, "T", nullptr, nullptr, nullptr, nullptr),
              HSIL_ERR_INVALID_ARG);

    hsil_destroy(h);
}

TEST_F(Validation_hsil_subscribe_streaming, InvalidProtocolReturnsInvalidArg)
{
    HsilHandle h = nullptr;
    ASSERT_EQ(hsil_create(0, nullptr, &h), HSIL_OK);

    auto cb = [](const char*, const HsilStreamingData*, void*){};
    EXPECT_EQ(hsil_subscribe_streaming(h, "T", nullptr, "INVALID_PROTO", cb, nullptr),
              HSIL_ERR_INVALID_ARG);

    hsil_destroy(h);
}

TEST_F(Validation_hsil_subscribe_streaming, CreatesReaderOnFirstCall)
{
    HsilHandle h = nullptr;
    ASSERT_EQ(hsil_create(0, nullptr, &h), HSIL_OK);

    auto cb = [](const char*, const HsilStreamingData*, void*){};
    ASSERT_EQ(hsil_subscribe_streaming(h, "CanBus", nullptr, nullptr, cb, nullptr), HSIL_OK);
    EXPECT_EQ(g_stubCfg.createStreamingReaderCallCount, 1);

    // Second subscribe on the same topic must reuse the reader
    ASSERT_EQ(hsil_subscribe_streaming(h, "CanBus", nullptr, nullptr, cb, nullptr), HSIL_OK);
    EXPECT_EQ(g_stubCfg.createStreamingReaderCallCount, 1);

    hsil_destroy(h);
}

TEST_F(Validation_hsil_subscribe_streaming, ReaderFailureReturnsDdsError)
{
    g_stubCfg.createReaderShouldFail = true;

    HsilHandle h = nullptr;
    ASSERT_EQ(hsil_create(0, nullptr, &h), HSIL_OK);

    auto cb = [](const char*, const HsilStreamingData*, void*){};
    EXPECT_EQ(hsil_subscribe_streaming(h, "T", nullptr, nullptr, cb, nullptr), HSIL_ERR_DDS);

    hsil_destroy(h);
}

TEST_F(Validation_hsil_subscribe_streaming, CallbackReceivedOnMatch)
{
    HsilHandle h = nullptr;
    ASSERT_EQ(hsil_create(0, nullptr, &h), HSIL_OK);

    std::atomic<int> hitCount{0};
    auto cb = [](const char*, const HsilStreamingData*, void* ud)
    {
        ++(*static_cast<std::atomic<int>*>(ud));
    };

    ASSERT_EQ(hsil_subscribe_streaming(h, "T", nullptr, nullptr, cb, &hitCount), HSIL_OK);

    ASSERT_NE(g_stubCfg.lastStreamingCallback, nullptr);
    HsilStreamingData sample{};
    std::strncpy(sample.protocol, "CAN_v1", 8);

    g_stubCfg.lastStreamingCallback("T", &sample, g_stubCfg.lastStreamingCallbackData);

    EXPECT_EQ(hitCount.load(), 1);

    hsil_destroy(h);
}

TEST_F(Validation_hsil_subscribe_streaming, ProtocolFilterWorks)
{
    HsilHandle h = nullptr;
    ASSERT_EQ(hsil_create(0, nullptr, &h), HSIL_OK);

    std::atomic<int> hitCount{0};
    auto cb = [](const char*, const HsilStreamingData*, void* ud)
    {
        ++(*static_cast<std::atomic<int>*>(ud));
    };

    // Subscribe with protocol="CAN_v1" — only CAN_v1 samples should be delivered
    ASSERT_EQ(hsil_subscribe_streaming(h, "T", nullptr, "CAN_v1", cb, &hitCount), HSIL_OK);

    HsilStreamingData mismatch{};
    std::strncpy(mismatch.protocol, "Eth_v1", 8);  // different protocol — must be filtered out
    g_stubCfg.lastStreamingCallback("T", &mismatch, g_stubCfg.lastStreamingCallbackData);
    EXPECT_EQ(hitCount.load(), 0);

    HsilStreamingData match{};
    std::strncpy(match.protocol, "CAN_v1", 8);  // matching protocol — must be delivered
    g_stubCfg.lastStreamingCallback("T", &match, g_stubCfg.lastStreamingCallbackData);
    EXPECT_EQ(hitCount.load(), 1);

    hsil_destroy(h);
}

TEST_F(Validation_hsil_subscribe_streaming, TwoCallbacksBothFire)
{
    HsilHandle h = nullptr;
    ASSERT_EQ(hsil_create(0, nullptr, &h), HSIL_OK);

    std::atomic<int> hitCount{0};
    auto cb = [](const char*, const HsilStreamingData*, void* ud)
    {
        ++(*static_cast<std::atomic<int>*>(ud));
    };

    // Two subscribe calls on the same topic — reader created once, both callbacks fire
    ASSERT_EQ(hsil_subscribe_streaming(h, "T", nullptr, nullptr, cb, &hitCount), HSIL_OK);
    ASSERT_EQ(hsil_subscribe_streaming(h, "T", nullptr, nullptr, cb, &hitCount), HSIL_OK);
    EXPECT_EQ(g_stubCfg.createStreamingReaderCallCount, 1);

    HsilStreamingData sample{};
    std::strncpy(sample.protocol, "CAN_v1", 8);
    g_stubCfg.lastStreamingCallback("T", &sample, g_stubCfg.lastStreamingCallbackData);
    EXPECT_EQ(hitCount.load(), 2);  // both callbacks fired

    hsil_destroy(h);
}

TEST_F(Validation_hsil_subscribe_streaming, DoesNotThrow)
{
    HsilHandle h = nullptr;
    ASSERT_EQ(hsil_create(0, nullptr, &h), HSIL_OK);
    g_stubCfg.shouldThrow = true;

    auto cb = [](const char*, const HsilStreamingData*, void*) {};
    EXPECT_NO_THROW({
        int rc = hsil_subscribe_streaming(h, "T", nullptr, nullptr, cb, nullptr);
        EXPECT_EQ(rc, HSIL_ERR_GENERIC);
    });
    g_stubCfg.shouldThrow = false;
    hsil_destroy(h);
}

// ===========================================================================
// hsil_create_grouped_publisher
// ===========================================================================

TEST_F(Validation_hsil_create_grouped_publisher, NullHandleReturnsInvalidArg)
{
    EXPECT_EQ(hsil_create_grouped_publisher(nullptr, "T", nullptr),
              HSIL_ERR_INVALID_ARG);
}

TEST_F(Validation_hsil_create_grouped_publisher, NullTopicReturnsInvalidArg)
{
    HsilHandle h = nullptr;
    ASSERT_EQ(hsil_create(0, nullptr, &h), HSIL_OK);

    EXPECT_EQ(hsil_create_grouped_publisher(h, nullptr, nullptr),
              HSIL_ERR_INVALID_ARG);

    hsil_destroy(h);
}

TEST_F(Validation_hsil_create_grouped_publisher, Success)
{
    HsilHandle h = nullptr;
    ASSERT_EQ(hsil_create(0, nullptr, &h), HSIL_OK);

    ASSERT_EQ(hsil_create_grouped_publisher(h, "T", nullptr), HSIL_OK);
    EXPECT_EQ(g_stubCfg.createGroupedWriterCallCount, 1);

    // The session owns the writer; it is destroyed on hsil_destroy.
    hsil_destroy(h);
    EXPECT_EQ(g_stubCfg.destroyWriterCallCount, 1);
}

TEST_F(Validation_hsil_create_grouped_publisher, SecondCallReusesWriter)
{
    HsilHandle h = nullptr;
    ASSERT_EQ(hsil_create(0, nullptr, &h), HSIL_OK);

    ASSERT_EQ(hsil_create_grouped_publisher(h, "T", nullptr), HSIL_OK);
    ASSERT_EQ(hsil_create_grouped_publisher(h, "T", nullptr), HSIL_OK);
    EXPECT_EQ(g_stubCfg.createGroupedWriterCallCount, 1);

    hsil_destroy(h);
}

TEST_F(Validation_hsil_create_grouped_publisher, WriterFailureReturnsDdsError)
{
    g_stubCfg.createWriterShouldFail = true;

    HsilHandle h = nullptr;
    ASSERT_EQ(hsil_create(0, nullptr, &h), HSIL_OK);

    EXPECT_EQ(hsil_create_grouped_publisher(h, "T", nullptr), HSIL_ERR_DDS);

    hsil_destroy(h);
}

TEST_F(Validation_hsil_create_grouped_publisher, DoesNotThrow)
{
    HsilHandle h = nullptr;
    ASSERT_EQ(hsil_create(0, nullptr, &h), HSIL_OK);
    g_stubCfg.shouldThrow = true;

    EXPECT_NO_THROW({
        int rc = hsil_create_grouped_publisher(h, "T", nullptr);
        EXPECT_EQ(rc, HSIL_ERR_GENERIC);
    });
    g_stubCfg.shouldThrow = false;
    hsil_destroy(h);
}

// ===========================================================================
// hsil_create_streaming_publisher
// ===========================================================================

TEST_F(Validation_hsil_create_streaming_publisher, NullHandleReturnsInvalidArg)
{
    EXPECT_EQ(hsil_create_streaming_publisher(nullptr, "T", nullptr),
              HSIL_ERR_INVALID_ARG);
}

TEST_F(Validation_hsil_create_streaming_publisher, NullTopicReturnsInvalidArg)
{
    HsilHandle h = nullptr;
    ASSERT_EQ(hsil_create(0, nullptr, &h), HSIL_OK);

    EXPECT_EQ(hsil_create_streaming_publisher(h, nullptr, nullptr),
              HSIL_ERR_INVALID_ARG);

    hsil_destroy(h);
}

TEST_F(Validation_hsil_create_streaming_publisher, Success)
{
    HsilHandle h = nullptr;
    ASSERT_EQ(hsil_create(0, nullptr, &h), HSIL_OK);

    ASSERT_EQ(hsil_create_streaming_publisher(h, "Can", nullptr), HSIL_OK);
    EXPECT_EQ(g_stubCfg.createStreamingWriterCallCount, 1);

    hsil_destroy(h);
    EXPECT_EQ(g_stubCfg.destroyWriterCallCount, 1);
}

TEST_F(Validation_hsil_create_streaming_publisher, SecondCallReusesWriter)
{
    HsilHandle h = nullptr;
    ASSERT_EQ(hsil_create(0, nullptr, &h), HSIL_OK);

    ASSERT_EQ(hsil_create_streaming_publisher(h, "T", nullptr), HSIL_OK);
    ASSERT_EQ(hsil_create_streaming_publisher(h, "T", nullptr), HSIL_OK);
    EXPECT_EQ(g_stubCfg.createStreamingWriterCallCount, 1);

    hsil_destroy(h);
}

TEST_F(Validation_hsil_create_streaming_publisher, WriterFailureReturnsDdsError)
{
    g_stubCfg.createWriterShouldFail = true;

    HsilHandle h = nullptr;
    ASSERT_EQ(hsil_create(0, nullptr, &h), HSIL_OK);

    EXPECT_EQ(hsil_create_streaming_publisher(h, "T", nullptr), HSIL_ERR_DDS);

    hsil_destroy(h);
}

TEST_F(Validation_hsil_create_streaming_publisher, DoesNotThrow)
{
    HsilHandle h = nullptr;
    ASSERT_EQ(hsil_create(0, nullptr, &h), HSIL_OK);
    g_stubCfg.shouldThrow = true;

    EXPECT_NO_THROW({
        int rc = hsil_create_streaming_publisher(h, "T", nullptr);
        EXPECT_EQ(rc, HSIL_ERR_GENERIC);
    });
    g_stubCfg.shouldThrow = false;
    hsil_destroy(h);
}

// ===========================================================================
// Topic type enforcement
// ===========================================================================

TEST_F(Validation_TopicTypeMismatch, GroupedThenStreamingPublisher)
{
    HsilHandle h = nullptr;
    ASSERT_EQ(hsil_create(0, nullptr, &h), HSIL_OK);

    ASSERT_EQ(hsil_create_grouped_publisher(h, "T", nullptr), HSIL_OK);

    EXPECT_EQ(hsil_create_streaming_publisher(h, "T", nullptr),
              HSIL_ERR_TOPIC_TYPE_MISMATCH);

    hsil_destroy(h);
}

TEST_F(Validation_TopicTypeMismatch, GroupedSubscriberThenStreamingSubscriber)
{
    HsilHandle h = nullptr;
    ASSERT_EQ(hsil_create(0, nullptr, &h), HSIL_OK);

    auto gcb = [](const char*, const HsilGroupedData*, void*){};
    ASSERT_EQ(hsil_subscribe_grouped(h, "T", nullptr, HSIL_ALL_IDS, gcb, nullptr), HSIL_OK);

    auto scb = [](const char*, const HsilStreamingData*, void*){};
    EXPECT_EQ(hsil_subscribe_streaming(h, "T", nullptr, nullptr, scb, nullptr),
              HSIL_ERR_TOPIC_TYPE_MISMATCH);

    hsil_destroy(h);
}

TEST_F(Validation_TopicTypeMismatch, SubscribeGroupedOnStreamingTopicReturnsMismatch)
{
    HsilHandle h = nullptr;
    ASSERT_EQ(hsil_create(0, nullptr, &h), HSIL_OK);

    // Subscribe streaming first, then try subscribe_grouped on same topic
    auto scb = [](const char*, const HsilStreamingData*, void*){};
    ASSERT_EQ(hsil_subscribe_streaming(h, "T", nullptr, nullptr, scb, nullptr), HSIL_OK);

    auto gcb = [](const char*, const HsilGroupedData*, void*){};
    EXPECT_EQ(hsil_subscribe_grouped(h, "T", nullptr, HSIL_ALL_IDS, gcb, nullptr),
              HSIL_ERR_TOPIC_TYPE_MISMATCH);

    hsil_destroy(h);
}

TEST_F(Validation_TopicTypeMismatch, SubscribeStreamingOnGroupedTopicReturnsMismatch)
{
    HsilHandle h = nullptr;
    ASSERT_EQ(hsil_create(0, nullptr, &h), HSIL_OK);

    // Subscribe grouped first, then try subscribe_streaming on same topic
    auto gcb = [](const char*, const HsilGroupedData*, void*){};
    ASSERT_EQ(hsil_subscribe_grouped(h, "T", nullptr, HSIL_ALL_IDS, gcb, nullptr), HSIL_OK);

    auto scb = [](const char*, const HsilStreamingData*, void*){};
    EXPECT_EQ(hsil_subscribe_streaming(h, "T", nullptr, nullptr, scb, nullptr),
              HSIL_ERR_TOPIC_TYPE_MISMATCH);

    hsil_destroy(h);
}

// ===========================================================================
// hsil_publish_grouped / hsil_publish_streaming – type enforcement
// ===========================================================================

TEST_F(Validation_hsil_publish_grouped, NullHandleReturnsInvalidArg)
{
    HsilGroupedData d{};
    EXPECT_EQ(hsil_publish_grouped(nullptr, "T", &d), HSIL_ERR_INVALID_ARG);
}

TEST_F(Validation_hsil_publish_grouped, NullTopicReturnsInvalidArg)
{
    HsilHandle h = nullptr;
    ASSERT_EQ(hsil_create(0, nullptr, &h), HSIL_OK);

    HsilGroupedData d{};
    EXPECT_EQ(hsil_publish_grouped(h, nullptr, &d), HSIL_ERR_INVALID_ARG);

    hsil_destroy(h);
}

TEST_F(Validation_hsil_publish_grouped, NullDataReturnsInvalidArg)
{
    HsilHandle h = nullptr;
    ASSERT_EQ(hsil_create(0, nullptr, &h), HSIL_OK);

    ASSERT_EQ(hsil_create_grouped_publisher(h, "T", nullptr), HSIL_OK);

    EXPECT_EQ(hsil_publish_grouped(h, "T", nullptr), HSIL_ERR_INVALID_ARG);

    hsil_destroy(h);
}

TEST_F(Validation_hsil_publish_grouped, UnknownTopicReturnsPubNotCreated)
{
    HsilHandle h = nullptr;
    ASSERT_EQ(hsil_create(0, nullptr, &h), HSIL_OK);

    HsilGroupedData d{};
    EXPECT_EQ(hsil_publish_grouped(h, "Nope", &d), HSIL_ERR_PUB_NOT_CREATED);

    hsil_destroy(h);
}

TEST_F(Validation_hsil_publish_grouped, OnStreamingTopicReturnsMismatch)
{
    HsilHandle h = nullptr;
    ASSERT_EQ(hsil_create(0, nullptr, &h), HSIL_OK);

    ASSERT_EQ(hsil_create_streaming_publisher(h, "T", nullptr), HSIL_OK);

    HsilGroupedData d{};
    EXPECT_EQ(hsil_publish_grouped(h, "T", &d), HSIL_ERR_TOPIC_TYPE_MISMATCH);

    hsil_destroy(h);
}

TEST_F(Validation_hsil_publish_grouped, Success)
{
    HsilHandle h = nullptr;
    ASSERT_EQ(hsil_create(0, nullptr, &h), HSIL_OK);

    ASSERT_EQ(hsil_create_grouped_publisher(h, "T", nullptr), HSIL_OK);

    HsilGroupedData d{};
    EXPECT_EQ(hsil_publish_grouped(h, "T", &d), HSIL_OK);
    EXPECT_EQ(g_stubCfg.writeGroupedCallCount, 1);

    hsil_destroy(h);
}

TEST_F(Validation_hsil_publish_grouped, DdsWriteFailureReturnsDdsError)
{
    g_stubCfg.writeShouldFail = true;

    HsilHandle h = nullptr;
    ASSERT_EQ(hsil_create(0, nullptr, &h), HSIL_OK);

    ASSERT_EQ(hsil_create_grouped_publisher(h, "T", nullptr), HSIL_OK);

    HsilGroupedData d{};
    EXPECT_EQ(hsil_publish_grouped(h, "T", &d), HSIL_ERR_DDS);

    hsil_destroy(h);
}

TEST_F(Validation_hsil_publish_grouped, DoesNotThrow)
{
    HsilHandle h = nullptr;
    ASSERT_EQ(hsil_create(0, nullptr, &h), HSIL_OK);
    ASSERT_EQ(hsil_create_grouped_publisher(h, "T", nullptr), HSIL_OK);
    g_stubCfg.shouldThrow = true;

    uint8_t buf[4] = {};
    HsilGroupedData sample{};
    sample.id       = 1u;
    sample.data     = buf;
    sample.dataSize = sizeof(buf);
    EXPECT_NO_THROW({
        int rc = hsil_publish_grouped(h, "T", &sample);
        EXPECT_EQ(rc, HSIL_ERR_GENERIC);
    });
    g_stubCfg.shouldThrow = false;
    hsil_destroy(h);
}

TEST_F(Validation_hsil_publish_streaming, NullHandleReturnsInvalidArg)
{
    HsilStreamingData d{};
    std::strncpy(d.protocol, "CAN_v1", 8);
    EXPECT_EQ(hsil_publish_streaming(nullptr, "T", &d), HSIL_ERR_INVALID_ARG);
}

TEST_F(Validation_hsil_publish_streaming, NullTopicReturnsInvalidArg)
{
    HsilHandle h = nullptr;
    ASSERT_EQ(hsil_create(0, nullptr, &h), HSIL_OK);

    HsilStreamingData d{};
    std::strncpy(d.protocol, "CAN_v1", 8);
    EXPECT_EQ(hsil_publish_streaming(h, nullptr, &d), HSIL_ERR_INVALID_ARG);

    hsil_destroy(h);
}

TEST_F(Validation_hsil_publish_streaming, NullDataReturnsInvalidArg)
{
    HsilHandle h = nullptr;
    ASSERT_EQ(hsil_create(0, nullptr, &h), HSIL_OK);

    ASSERT_EQ(hsil_create_streaming_publisher(h, "T", nullptr), HSIL_OK);

    EXPECT_EQ(hsil_publish_streaming(h, "T", nullptr), HSIL_ERR_INVALID_ARG);

    hsil_destroy(h);
}

TEST_F(Validation_hsil_publish_streaming, InvalidProtocolReturnsInvalidArg)
{
    HsilHandle h = nullptr;
    ASSERT_EQ(hsil_create(0, nullptr, &h), HSIL_OK);

    ASSERT_EQ(hsil_create_streaming_publisher(h, "T", nullptr), HSIL_OK);

    // Unrecognised protocol string — not in ["generic", "CAN_v1", "Eth_v1", "EthJ_v1"]
    HsilStreamingData d{};
    std::strncpy(d.protocol, "UNKNOWN", 8);
    EXPECT_EQ(hsil_publish_streaming(h, "T", &d), HSIL_ERR_INVALID_ARG);

    hsil_destroy(h);
}

TEST_F(Validation_hsil_publish_streaming, EmptyProtocolReturnsInvalidArg)
{
    HsilHandle h = nullptr;
    ASSERT_EQ(hsil_create(0, nullptr, &h), HSIL_OK);

    ASSERT_EQ(hsil_create_streaming_publisher(h, "T", nullptr), HSIL_OK);

    // Default-initialised HsilStreamingData has protocol = "" which is not valid
    HsilStreamingData d{};
    EXPECT_EQ(hsil_publish_streaming(h, "T", &d), HSIL_ERR_INVALID_ARG);

    hsil_destroy(h);
}

TEST_F(Validation_hsil_publish_streaming, UnknownTopicReturnsPubNotCreated)
{
    HsilHandle h = nullptr;
    ASSERT_EQ(hsil_create(0, nullptr, &h), HSIL_OK);

    HsilStreamingData d{};
    std::strncpy(d.protocol, "CAN_v1", 8);
    EXPECT_EQ(hsil_publish_streaming(h, "Nope", &d), HSIL_ERR_PUB_NOT_CREATED);

    hsil_destroy(h);
}

TEST_F(Validation_hsil_publish_streaming, OnGroupedTopicReturnsMismatch)
{
    HsilHandle h = nullptr;
    ASSERT_EQ(hsil_create(0, nullptr, &h), HSIL_OK);

    ASSERT_EQ(hsil_create_grouped_publisher(h, "T", nullptr), HSIL_OK);

    HsilStreamingData d{};
    std::strncpy(d.protocol, "CAN_v1", 8);
    EXPECT_EQ(hsil_publish_streaming(h, "T", &d), HSIL_ERR_TOPIC_TYPE_MISMATCH);

    hsil_destroy(h);
}

TEST_F(Validation_hsil_publish_streaming, AllValidProtocolsAreAccepted)
{
    static const char* const kValidProtocols[] = { "generic", "CAN_v1", "Eth_v1", "EthJ_v1" };

    for (const char* proto : kValidProtocols)
    {
        g_stubCfg.reset();

        HsilHandle h = nullptr;
        ASSERT_EQ(hsil_create(0, nullptr, &h), HSIL_OK);

        ASSERT_EQ(hsil_create_streaming_publisher(h, "T", nullptr), HSIL_OK);

        HsilStreamingData d{};
        std::strncpy(d.protocol, proto, sizeof(d.protocol));
        EXPECT_EQ(hsil_publish_streaming(h, "T", &d), HSIL_OK) << "protocol: " << proto;
        EXPECT_EQ(g_stubCfg.writeStreamingCallCount, 1) << "protocol: " << proto;

        hsil_destroy(h);
    }
}

TEST_F(Validation_hsil_publish_streaming, DdsWriteFailureReturnsDdsError)
{
    g_stubCfg.writeShouldFail = true;

    HsilHandle h = nullptr;
    ASSERT_EQ(hsil_create(0, nullptr, &h), HSIL_OK);

    ASSERT_EQ(hsil_create_streaming_publisher(h, "T", nullptr), HSIL_OK);

    HsilStreamingData d{};
    std::strncpy(d.protocol, "CAN_v1", 8);
    EXPECT_EQ(hsil_publish_streaming(h, "T", &d), HSIL_ERR_DDS);

    hsil_destroy(h);
}

TEST_F(Validation_hsil_publish_streaming, DoesNotThrow)
{
    HsilHandle h = nullptr;
    ASSERT_EQ(hsil_create(0, nullptr, &h), HSIL_OK);
    ASSERT_EQ(hsil_create_streaming_publisher(h, "T", nullptr), HSIL_OK);
    g_stubCfg.shouldThrow = true;

    uint8_t payload[4] = {};
    HsilStreamingData sample{};
    std::strncpy(sample.protocol, "generic", sizeof(sample.protocol));
    sample.data     = payload;
    sample.dataSize = sizeof(payload);
    EXPECT_NO_THROW({
        int rc = hsil_publish_streaming(h, "T", &sample);
        EXPECT_EQ(rc, HSIL_ERR_GENERIC);
    });
    g_stubCfg.shouldThrow = false;
    hsil_destroy(h);
}

// ===========================================================================
// hsil_get_publisher_status / hsil_get_subscriber_status
// ===========================================================================

TEST_F(Validation_hsil_endpoint_get_status, NullArgsReturnInvalidArg)
{
    HsilHandle h = nullptr;
    ASSERT_EQ(hsil_create(0, nullptr, &h), HSIL_OK);
    ASSERT_EQ(hsil_create_grouped_publisher(h, "T", nullptr), HSIL_OK);

    HsilEndpointStatus status{};
    EXPECT_EQ(hsil_get_publisher_status(nullptr, "T", &status), HSIL_ERR_INVALID_ARG);
    EXPECT_EQ(hsil_get_publisher_status(h, nullptr, &status), HSIL_ERR_INVALID_ARG);
    EXPECT_EQ(hsil_get_publisher_status(h, "T", nullptr), HSIL_ERR_INVALID_ARG);

    EXPECT_EQ(hsil_get_subscriber_status(nullptr, "T", &status), HSIL_ERR_INVALID_ARG);
    EXPECT_EQ(hsil_get_subscriber_status(h, nullptr, &status), HSIL_ERR_INVALID_ARG);
    EXPECT_EQ(hsil_get_subscriber_status(h, "T", nullptr), HSIL_ERR_INVALID_ARG);

    // No vendor call must happen when validation fails.
    EXPECT_EQ(g_stubCfg.getEndpointStatusCallCount, 0);

    hsil_destroy(h);
}

TEST_F(Validation_hsil_endpoint_get_status, UnknownTopicReturnsUnknownTopic)
{
    HsilHandle h = nullptr;
    ASSERT_EQ(hsil_create(0, nullptr, &h), HSIL_OK);

    HsilEndpointStatus status{};
    EXPECT_EQ(hsil_get_publisher_status(h, "NeverUsed", &status), HSIL_ERR_UNKNOWN_TOPIC);
    EXPECT_EQ(hsil_get_subscriber_status(h, "NeverUsed", &status), HSIL_ERR_UNKNOWN_TOPIC);

    hsil_destroy(h);
}

TEST_F(Validation_hsil_endpoint_get_status, MissingEndpointReturnsNotCreated)
{
    HsilHandle h = nullptr;
    ASSERT_EQ(hsil_create(0, nullptr, &h), HSIL_OK);

    // "PubOnly" has a writer but no reader; "SubOnly" has a reader but no writer.
    ASSERT_EQ(hsil_create_grouped_publisher(h, "PubOnly", nullptr), HSIL_OK);
    auto cb = [](const char*, const HsilGroupedData*, void*) {};
    ASSERT_EQ(hsil_subscribe_grouped(h, "SubOnly", nullptr, HSIL_ALL_IDS, cb, nullptr), HSIL_OK);

    HsilEndpointStatus status{};
    EXPECT_EQ(hsil_get_subscriber_status(h, "PubOnly", &status), HSIL_ERR_SUB_NOT_CREATED);
    EXPECT_EQ(hsil_get_publisher_status(h, "SubOnly", &status), HSIL_ERR_PUB_NOT_CREATED);

    hsil_destroy(h);
}

TEST_F(Validation_hsil_endpoint_get_status, ReportsVendorStatusForMatchingEndpoint)
{
    HsilHandle h = nullptr;
    ASSERT_EQ(hsil_create(0, nullptr, &h), HSIL_OK);

    // One topic that is both published and subscribed, so the only thing that
    // distinguishes the two calls is which endpoint they ask the vendor about.
    ASSERT_EQ(hsil_create_grouped_publisher(h, "T", nullptr), HSIL_OK);
    auto cb = [](const char*, const HsilGroupedData*, void*) {};
    ASSERT_EQ(hsil_subscribe_grouped(h, "T", nullptr, HSIL_ALL_IDS, cb, nullptr), HSIL_OK);

    g_stubCfg.statusToReturn.matchedCount            = 2;
    g_stubCfg.statusToReturn.qosMismatchCount        = 1;
    g_stubCfg.statusToReturn.lastMismatchedQosPolicy = HSIL_QOS_MISMATCH_RELIABILITY;

    HsilEndpointStatus status{};
    ASSERT_EQ(hsil_get_publisher_status(h, "T", &status), HSIL_OK);
    EXPECT_EQ(g_stubCfg.lastGetStatusIsWriter, 1);
    EXPECT_EQ(status.matchedCount, 2);
    EXPECT_EQ(status.qosMismatchCount, 1);
    EXPECT_EQ(status.lastMismatchedQosPolicy, HSIL_QOS_MISMATCH_RELIABILITY);

    status = HsilEndpointStatus{};
    ASSERT_EQ(hsil_get_subscriber_status(h, "T", &status), HSIL_OK);
    EXPECT_EQ(g_stubCfg.lastGetStatusIsWriter, 0);
    EXPECT_EQ(status.matchedCount, 2);
    EXPECT_EQ(status.qosMismatchCount, 1);
    EXPECT_EQ(status.lastMismatchedQosPolicy, HSIL_QOS_MISMATCH_RELIABILITY);

    EXPECT_EQ(g_stubCfg.getEndpointStatusCallCount, 2);

    hsil_destroy(h);
}

TEST_F(Validation_hsil_endpoint_get_status, VendorFailureReturnsDdsError)
{
    HsilHandle h = nullptr;
    ASSERT_EQ(hsil_create(0, nullptr, &h), HSIL_OK);
    ASSERT_EQ(hsil_create_grouped_publisher(h, "T", nullptr), HSIL_OK);

    g_stubCfg.getStatusShouldFail = true;

    HsilEndpointStatus status{};
    EXPECT_EQ(hsil_get_publisher_status(h, "T", &status), HSIL_ERR_DDS);

    hsil_destroy(h);
}

TEST_F(Validation_hsil_endpoint_get_status, DoesNotThrow)
{
    HsilHandle h = nullptr;
    ASSERT_EQ(hsil_create(0, nullptr, &h), HSIL_OK);
    ASSERT_EQ(hsil_create_grouped_publisher(h, "T", nullptr), HSIL_OK);
    g_stubCfg.shouldThrow = true;

    HsilEndpointStatus status{};
    EXPECT_NO_THROW({
        int rc = hsil_get_publisher_status(h, "T", &status);
        EXPECT_EQ(rc, HSIL_ERR_GENERIC);
    });
    g_stubCfg.shouldThrow = false;
    hsil_destroy(h);
}

// ===========================================================================
// QoS validation – invalid fields are clamped to defaults by the API layer;
// API calls always succeed and the sanitised QoS is forwarded to the stub.
// ===========================================================================

TEST_F(Validation_QoS, NullQosSanitizedToDefault)
{
    HsilHandle h = nullptr;
    ASSERT_EQ(hsil_create(0, nullptr, &h), HSIL_OK);

    // Passing nullptr as qos must be accepted; default QoS is applied.
    ASSERT_EQ(hsil_create_grouped_publisher(h, "T", nullptr), HSIL_OK);

    EXPECT_EQ(g_stubCfg.lastWriterQos.priority,    HSIL_QOS_PRIORITY_NORMAL);
    EXPECT_EQ(g_stubCfg.lastWriterQos.queueLength, 1);
    EXPECT_EQ(g_stubCfg.lastWriterQos.reliability, HSIL_QOS_RELIABILITY_FAST);

    hsil_destroy(h);
}

TEST_F(Validation_QoS, ValidQosIsForwardedUnchanged)
{
    HsilHandle h = nullptr;
    ASSERT_EQ(hsil_create(0, nullptr, &h), HSIL_OK);

    HsilQos q{};
    q.priority    = HSIL_QOS_PRIORITY_HIGH;
    q.queueLength = 8;
    q.reliability = HSIL_QOS_RELIABILITY_RELIABLE;

    ASSERT_EQ(hsil_create_grouped_publisher(h, "T", &q), HSIL_OK);

    EXPECT_EQ(g_stubCfg.lastWriterQos.priority,    HSIL_QOS_PRIORITY_HIGH);
    EXPECT_EQ(g_stubCfg.lastWriterQos.queueLength, 8);
    EXPECT_EQ(g_stubCfg.lastWriterQos.reliability, HSIL_QOS_RELIABILITY_RELIABLE);

    hsil_destroy(h);
}

TEST_F(Validation_QoS, InvalidPriorityClampedToNormal)
{
    HsilHandle h = nullptr;
    ASSERT_EQ(hsil_create(0, nullptr, &h), HSIL_OK);

    HsilQos q{};
    q.priority    = static_cast<HsilQosPriority>(99);  // out-of-range enum value
    q.queueLength = 1;
    q.reliability = HSIL_QOS_RELIABILITY_FAST;

    // API must succeed; invalid priority is clamped to NORMAL
    ASSERT_EQ(hsil_create_grouped_publisher(h, "T", &q), HSIL_OK);
    EXPECT_EQ(g_stubCfg.lastWriterQos.priority, HSIL_QOS_PRIORITY_NORMAL);

    hsil_destroy(h);
}

TEST_F(Validation_QoS, InvalidQueueLengthClampedToOne)
{
    HsilHandle h = nullptr;
    ASSERT_EQ(hsil_create(0, nullptr, &h), HSIL_OK);

    HsilQos q{};
    q.priority    = HSIL_QOS_PRIORITY_NORMAL;
    q.queueLength = -5;  // invalid; must be >= 1
    q.reliability = HSIL_QOS_RELIABILITY_FAST;

    ASSERT_EQ(hsil_create_grouped_publisher(h, "T", &q), HSIL_OK);
    EXPECT_EQ(g_stubCfg.lastWriterQos.queueLength, 1);

    hsil_destroy(h);
}

TEST_F(Validation_QoS, InvalidReliabilityClampedToFast)
{
    HsilHandle h = nullptr;
    ASSERT_EQ(hsil_create(0, nullptr, &h), HSIL_OK);

    HsilQos q{};
    q.priority    = HSIL_QOS_PRIORITY_NORMAL;
    q.queueLength = 1;
    q.reliability = static_cast<HsilQosReliability>(99);  // out-of-range enum value

    ASSERT_EQ(hsil_create_grouped_publisher(h, "T", &q), HSIL_OK);
    EXPECT_EQ(g_stubCfg.lastWriterQos.reliability, HSIL_QOS_RELIABILITY_FAST);

    hsil_destroy(h);
}

TEST_F(Validation_QoS, SubscriberQosIsForwardedToReader)
{
    HsilHandle h = nullptr;
    ASSERT_EQ(hsil_create(0, nullptr, &h), HSIL_OK);

    HsilQos q{};
    q.priority    = HSIL_QOS_PRIORITY_LOW;
    q.queueLength = 4;
    q.reliability = HSIL_QOS_RELIABILITY_RELIABLE;

    auto cb = [](const char*, const HsilGroupedData*, void*){};
    ASSERT_EQ(hsil_subscribe_grouped(h, "T", &q, HSIL_ALL_IDS, cb, nullptr), HSIL_OK);

    EXPECT_EQ(g_stubCfg.lastReaderQos.priority,    HSIL_QOS_PRIORITY_LOW);
    EXPECT_EQ(g_stubCfg.lastReaderQos.queueLength, 4);
    EXPECT_EQ(g_stubCfg.lastReaderQos.reliability, HSIL_QOS_RELIABILITY_RELIABLE);

    hsil_destroy(h);
}

// ===========================================================================
// Helper: open a config-mode session using the dedicated API test fixture.
// The stub DDS is used so no live network is needed.
//
// Signals available (topic "VehicleSignals", group id 1):
//   vehicle.speed        float32  offset  0  count 1  (bytes  0– 3)
//   vehicle.rpm          float32  offset  4  count 1  (bytes  4– 7)
//   vehicle.gearPos      int32    offset  8  count 1  (bytes  8–11)
//   vehicle.wheelSpeeds  float32  offset 12  count 4  (bytes 12–27)
// ===========================================================================

static const char* kFixture = HSIL_TESTS_DIR "/api/fixtures/api_signals.json";
static const char* kTopic   = "VehicleSignals";
static const uint32_t kGroupId = 1u;

// Build a GroupedData packet backed by a 28-byte buffer initialised to zero.
struct SampleBuffer
{
    uint8_t         buf[28]{};
    HsilGroupedData sample{};

    SampleBuffer()
    {
        sample.id       = kGroupId;
        sample.data     = buf;
        sample.dataSize = sizeof(buf);
    }
};

// ===========================================================================
// hsil_check_signal_conversion
// ===========================================================================

TEST_F(Validation_hsil_check_signal_conversion, SameTypeIsOk)
{
    EXPECT_EQ(hsil_check_signal_conversion(HSIL_SIGNAL_INT32,   HSIL_SIGNAL_INT32),   HSIL_SIGNAL_CONV_OK);
    EXPECT_EQ(hsil_check_signal_conversion(HSIL_SIGNAL_FLOAT64, HSIL_SIGNAL_FLOAT64), HSIL_SIGNAL_CONV_OK);
    EXPECT_EQ(hsil_check_signal_conversion(HSIL_SIGNAL_BOOL,    HSIL_SIGNAL_BOOL),    HSIL_SIGNAL_CONV_OK);
    EXPECT_EQ(hsil_check_signal_conversion(HSIL_SIGNAL_BYTE,    HSIL_SIGNAL_BYTE),    HSIL_SIGNAL_CONV_OK);
    EXPECT_EQ(hsil_check_signal_conversion(HSIL_SIGNAL_CHAR,    HSIL_SIGNAL_CHAR),    HSIL_SIGNAL_CONV_OK);
}

TEST_F(Validation_hsil_check_signal_conversion, LosslessWidening)
{
    // Unsigned promotion into wider unsigned – lossless per spec (✔)
    EXPECT_EQ(hsil_check_signal_conversion(HSIL_SIGNAL_UINT8,  HSIL_SIGNAL_UINT16),  HSIL_SIGNAL_CONV_OK);
    EXPECT_EQ(hsil_check_signal_conversion(HSIL_SIGNAL_UINT16, HSIL_SIGNAL_UINT32),  HSIL_SIGNAL_CONV_OK);
    EXPECT_EQ(hsil_check_signal_conversion(HSIL_SIGNAL_UINT32, HSIL_SIGNAL_UINT64),  HSIL_SIGNAL_CONV_OK);
    // Signed promotion – lossless
    EXPECT_EQ(hsil_check_signal_conversion(HSIL_SIGNAL_INT8,   HSIL_SIGNAL_INT16),   HSIL_SIGNAL_CONV_OK);
    EXPECT_EQ(hsil_check_signal_conversion(HSIL_SIGNAL_INT16,  HSIL_SIGNAL_INT32),   HSIL_SIGNAL_CONV_OK);
    EXPECT_EQ(hsil_check_signal_conversion(HSIL_SIGNAL_INT32,  HSIL_SIGNAL_INT64),   HSIL_SIGNAL_CONV_OK);
    // float32 → float64 is lossless
    EXPECT_EQ(hsil_check_signal_conversion(HSIL_SIGNAL_FLOAT32, HSIL_SIGNAL_FLOAT64), HSIL_SIGNAL_CONV_OK);
    // uint8 can represent all values of int16 and above (signed widening of unsigned)
    EXPECT_EQ(hsil_check_signal_conversion(HSIL_SIGNAL_UINT8,  HSIL_SIGNAL_INT16),   HSIL_SIGNAL_CONV_OK);
}

TEST_F(Validation_hsil_check_signal_conversion, LossyNarrowing)
{
    // Narrowing – value may be truncated (⚠️)
    EXPECT_EQ(hsil_check_signal_conversion(HSIL_SIGNAL_INT32,   HSIL_SIGNAL_INT16),   HSIL_SIGNAL_CONV_LOSSY);
    EXPECT_EQ(hsil_check_signal_conversion(HSIL_SIGNAL_UINT64,  HSIL_SIGNAL_UINT8),   HSIL_SIGNAL_CONV_LOSSY);
    EXPECT_EQ(hsil_check_signal_conversion(HSIL_SIGNAL_FLOAT64, HSIL_SIGNAL_FLOAT32), HSIL_SIGNAL_CONV_LOSSY);
    // Integer → float may lose precision for large integers
    EXPECT_EQ(hsil_check_signal_conversion(HSIL_SIGNAL_INT32,   HSIL_SIGNAL_FLOAT32), HSIL_SIGNAL_CONV_LOSSY);
    // Signed ↔ unsigned same width
    EXPECT_EQ(hsil_check_signal_conversion(HSIL_SIGNAL_INT8,    HSIL_SIGNAL_UINT8),   HSIL_SIGNAL_CONV_LOSSY);
    EXPECT_EQ(hsil_check_signal_conversion(HSIL_SIGNAL_UINT8,   HSIL_SIGNAL_INT8),    HSIL_SIGNAL_CONV_LOSSY);
}

TEST_F(Validation_hsil_check_signal_conversion, InvalidCombinations)
{
    // float ↔ bool is not permitted
    EXPECT_EQ(hsil_check_signal_conversion(HSIL_SIGNAL_FLOAT32, HSIL_SIGNAL_BOOL),  HSIL_SIGNAL_CONV_INVALID);
    EXPECT_EQ(hsil_check_signal_conversion(HSIL_SIGNAL_FLOAT64, HSIL_SIGNAL_BOOL),  HSIL_SIGNAL_CONV_INVALID);
    // byte ↔ any integer is not permitted (byte is opaque)
    EXPECT_EQ(hsil_check_signal_conversion(HSIL_SIGNAL_BYTE,    HSIL_SIGNAL_INT32), HSIL_SIGNAL_CONV_INVALID);
    EXPECT_EQ(hsil_check_signal_conversion(HSIL_SIGNAL_INT32,   HSIL_SIGNAL_BYTE),  HSIL_SIGNAL_CONV_INVALID);
    // char ↔ integer is not permitted
    EXPECT_EQ(hsil_check_signal_conversion(HSIL_SIGNAL_CHAR,    HSIL_SIGNAL_INT8),  HSIL_SIGNAL_CONV_INVALID);
    EXPECT_EQ(hsil_check_signal_conversion(HSIL_SIGNAL_INT8,    HSIL_SIGNAL_CHAR),  HSIL_SIGNAL_CONV_INVALID);
    // Out-of-range type index
    EXPECT_EQ(hsil_check_signal_conversion(static_cast<HsilSignalType>(99), HSIL_SIGNAL_INT32), HSIL_SIGNAL_CONV_INVALID);
}

TEST_F(Validation_hsil_check_signal_conversion, BoolToIntegralIsOk)
{
    // Spec: bool → any integer type is silent (0/1 coerce)
    EXPECT_EQ(hsil_check_signal_conversion(HSIL_SIGNAL_BOOL, HSIL_SIGNAL_INT8),   HSIL_SIGNAL_CONV_OK);
    EXPECT_EQ(hsil_check_signal_conversion(HSIL_SIGNAL_BOOL, HSIL_SIGNAL_UINT32), HSIL_SIGNAL_CONV_OK);
    EXPECT_EQ(hsil_check_signal_conversion(HSIL_SIGNAL_BOOL, HSIL_SIGNAL_INT64),  HSIL_SIGNAL_CONV_OK);
}

TEST_F(Validation_hsil_check_signal_conversion, DoesNotThrow)
{
    EXPECT_NO_THROW(hsil_check_signal_conversion(HSIL_SIGNAL_FLOAT64, HSIL_SIGNAL_FLOAT32));
    // Out-of-range enum values – should return HSIL_SIGNAL_CONV_INVALID without throwing
    EXPECT_NO_THROW({
        HsilSignalConversion r = hsil_check_signal_conversion(
            static_cast<HsilSignalType>(99), static_cast<HsilSignalType>(99));
        EXPECT_EQ(r, HSIL_SIGNAL_CONV_INVALID);
    });
}

// ===========================================================================
// hsil_read_signal_by_name
// ===========================================================================

TEST_F(Validation_hsil_read_signal_by_name, NullArgsReturnInvalidArg)
{
    HsilHandle h = nullptr;
    ASSERT_EQ(hsil_create_from_config(kFixture, nullptr, &h), HSIL_OK);

    SampleBuffer sb;
    HsilSignalValue val{};
    HsilSignalType  type{};

    EXPECT_EQ(hsil_read_signal_by_name(nullptr, kTopic, &sb.sample, "vehicle.speed", 0, &val, &type),
              HSIL_ERR_INVALID_ARG);
    EXPECT_EQ(hsil_read_signal_by_name(h, nullptr, &sb.sample, "vehicle.speed", 0, &val, &type),
              HSIL_ERR_INVALID_ARG);
    EXPECT_EQ(hsil_read_signal_by_name(h, kTopic, nullptr, "vehicle.speed", 0, &val, &type),
              HSIL_ERR_INVALID_ARG);
    EXPECT_EQ(hsil_read_signal_by_name(h, kTopic, &sb.sample, nullptr, 0, &val, &type),
              HSIL_ERR_INVALID_ARG);
    EXPECT_EQ(hsil_read_signal_by_name(h, kTopic, &sb.sample, "vehicle.speed", 0, nullptr, &type),
              HSIL_ERR_INVALID_ARG);
    EXPECT_EQ(hsil_read_signal_by_name(h, kTopic, &sb.sample, "vehicle.speed", 0, &val, nullptr),
              HSIL_ERR_INVALID_ARG);

    hsil_destroy(h);
}

TEST_F(Validation_hsil_read_signal_by_name, UnknownTopicReturnsUnknownTopic)
{
    HsilHandle h = nullptr;
    ASSERT_EQ(hsil_create_from_config(kFixture, nullptr, &h), HSIL_OK);

    SampleBuffer sb;
    HsilSignalValue val{};
    HsilSignalType  type{};

    EXPECT_EQ(hsil_read_signal_by_name(h, "NoSuchTopic", &sb.sample, "vehicle.speed", 0, &val, &type),
              HSIL_ERR_UNKNOWN_TOPIC);

    hsil_destroy(h);
}

TEST_F(Validation_hsil_read_signal_by_name, UnknownSignalReturnsUnknownSignal)
{
    HsilHandle h = nullptr;
    ASSERT_EQ(hsil_create_from_config(kFixture, nullptr, &h), HSIL_OK);

    SampleBuffer sb;
    HsilSignalValue val{};
    HsilSignalType  type{};

    EXPECT_EQ(hsil_read_signal_by_name(h, kTopic, &sb.sample, "no.such.signal", 0, &val, &type),
              HSIL_ERR_UNKNOWN_SIGNAL);

    hsil_destroy(h);
}

TEST_F(Validation_hsil_read_signal_by_name, OutOfRangeArrayIndexReturnsGenericError)
{
    HsilHandle h = nullptr;
    ASSERT_EQ(hsil_create_from_config(kFixture, nullptr, &h), HSIL_OK);

    SampleBuffer sb;
    HsilSignalValue val{};
    HsilSignalType  type{};

    // vehicle.speed is a scalar (count=1); arrayIndex=1 is out of range
    EXPECT_EQ(hsil_read_signal_by_name(h, kTopic, &sb.sample, "vehicle.speed", 1, &val, &type),
              HSIL_ERR_GENERIC);

    hsil_destroy(h);
}

TEST_F(Validation_hsil_read_signal_by_name, ReadsFloat32CorrectlyFromBuffer)
{
    HsilHandle h = nullptr;
    ASSERT_EQ(hsil_create_from_config(kFixture, nullptr, &h), HSIL_OK);

    SampleBuffer sb;
    const float expected = 123.456f;
    std::memcpy(&sb.buf[0], &expected, sizeof(float)); // vehicle.speed is at offset 0

    HsilSignalValue val{};
    HsilSignalType  type{};
    ASSERT_EQ(hsil_read_signal_by_name(h, kTopic, &sb.sample, "vehicle.speed", 0, &val, &type), HSIL_OK);

    EXPECT_EQ(type, HSIL_SIGNAL_FLOAT32);
    EXPECT_FLOAT_EQ(val.asFloat32, expected);

    hsil_destroy(h);
}

TEST_F(Validation_hsil_read_signal_by_name, ReadsInt32CorrectlyFromBuffer)
{
    HsilHandle h = nullptr;
    ASSERT_EQ(hsil_create_from_config(kFixture, nullptr, &h), HSIL_OK);

    SampleBuffer sb;
    const int32_t expected = -42;
    std::memcpy(&sb.buf[8], &expected, sizeof(int32_t)); // vehicle.gearPos is at offset 8

    HsilSignalValue val{};
    HsilSignalType  type{};
    ASSERT_EQ(hsil_read_signal_by_name(h, kTopic, &sb.sample, "vehicle.gearPos", 0, &val, &type), HSIL_OK);

    EXPECT_EQ(type, HSIL_SIGNAL_INT32);
    EXPECT_EQ(val.asInt32, expected);

    hsil_destroy(h);
}

TEST_F(Validation_hsil_read_signal_by_name, NullDataBufferReturnsInvalidArg)
{
    HsilHandle h = nullptr;
    ASSERT_EQ(hsil_create_from_config(kFixture, nullptr, &h), HSIL_OK);

    HsilGroupedData pkt{};
    pkt.id       = kGroupId;
    pkt.data     = nullptr; // NULL buffer → invalid arg
    pkt.dataSize = 0;

    HsilSignalValue val{};
    HsilSignalType  type{};
    EXPECT_EQ(hsil_read_signal_by_name(h, kTopic, &pkt, "vehicle.speed", 0, &val, &type),
              HSIL_ERR_INVALID_ARG);

    hsil_destroy(h);
}

TEST_F(Validation_hsil_read_signal_by_name, ReadsEachArrayElementByIndex)
{
    HsilHandle h = nullptr;
    ASSERT_EQ(hsil_create_from_config(kFixture, nullptr, &h), HSIL_OK);

    SampleBuffer sb;
    const float speeds[4] = { 10.0f, 20.0f, 30.0f, 40.0f };
    std::memcpy(&sb.buf[12], speeds, sizeof(speeds));

    for (size_t i = 0; i < 4; ++i)
    {
        HsilSignalValue val{};
        HsilSignalType  type{};
        ASSERT_EQ(hsil_read_signal_by_name(h, kTopic, &sb.sample, "vehicle.wheelSpeeds", i, &val, &type), HSIL_OK)
            << "arrayIndex=" << i;
        EXPECT_EQ(type, HSIL_SIGNAL_FLOAT32) << "arrayIndex=" << i;
        EXPECT_FLOAT_EQ(val.asFloat32, speeds[i]) << "arrayIndex=" << i;
    }

    hsil_destroy(h);
}

TEST_F(Validation_hsil_read_signal_by_name, ArrayOutOfRangeReturnsGenericError)
{
    HsilHandle h = nullptr;
    ASSERT_EQ(hsil_create_from_config(kFixture, nullptr, &h), HSIL_OK);

    SampleBuffer sb;
    HsilSignalValue val{};
    HsilSignalType  type{};

    // vehicle.wheelSpeeds has count=4; index 4 is one past the end
    EXPECT_EQ(hsil_read_signal_by_name(h, kTopic, &sb.sample, "vehicle.wheelSpeeds", 4, &val, &type),
              HSIL_ERR_GENERIC);

    hsil_destroy(h);
}

// ===========================================================================
// hsil_write_signal_by_name
// ===========================================================================

TEST_F(Validation_hsil_write_signal_by_name, NullArgsReturnInvalidArg)
{
    HsilHandle h = nullptr;
    ASSERT_EQ(hsil_create_from_config(kFixture, nullptr, &h), HSIL_OK);

    SampleBuffer sb;
    HsilSignalValue val{};
    val.asFloat32 = 1.0f;

    EXPECT_EQ(hsil_write_signal_by_name(nullptr, kTopic, &sb.sample, "vehicle.speed", 0, val, HSIL_SIGNAL_FLOAT32),
              HSIL_ERR_INVALID_ARG);
    EXPECT_EQ(hsil_write_signal_by_name(h, nullptr, &sb.sample, "vehicle.speed", 0, val, HSIL_SIGNAL_FLOAT32),
              HSIL_ERR_INVALID_ARG);
    EXPECT_EQ(hsil_write_signal_by_name(h, kTopic, nullptr, "vehicle.speed", 0, val, HSIL_SIGNAL_FLOAT32),
              HSIL_ERR_INVALID_ARG);
    EXPECT_EQ(hsil_write_signal_by_name(h, kTopic, &sb.sample, nullptr, 0, val, HSIL_SIGNAL_FLOAT32),
              HSIL_ERR_INVALID_ARG);

    hsil_destroy(h);
}

TEST_F(Validation_hsil_write_signal_by_name, UnknownTopicReturnsUnknownTopic)
{
    HsilHandle h = nullptr;
    ASSERT_EQ(hsil_create_from_config(kFixture, nullptr, &h), HSIL_OK);

    SampleBuffer sb;
    HsilSignalValue val{};
    val.asFloat32 = 0.0f;
    EXPECT_EQ(hsil_write_signal_by_name(h, "NoSuchTopic", &sb.sample, "vehicle.speed", 0, val, HSIL_SIGNAL_FLOAT32),
              HSIL_ERR_UNKNOWN_TOPIC);

    hsil_destroy(h);
}

TEST_F(Validation_hsil_write_signal_by_name, UnknownSignalReturnsUnknownSignal)
{
    HsilHandle h = nullptr;
    ASSERT_EQ(hsil_create_from_config(kFixture, nullptr, &h), HSIL_OK);

    SampleBuffer sb;
    HsilSignalValue val{};
    EXPECT_EQ(hsil_write_signal_by_name(h, kTopic, &sb.sample, "no.such.signal", 0, val, HSIL_SIGNAL_FLOAT32),
              HSIL_ERR_UNKNOWN_SIGNAL);

    hsil_destroy(h);
}

TEST_F(Validation_hsil_write_signal_by_name, InvalidConversionReturnsConversionError)
{
    HsilHandle h = nullptr;
    ASSERT_EQ(hsil_create_from_config(kFixture, nullptr, &h), HSIL_OK);

    SampleBuffer sb;
    HsilSignalValue val{};
    val.asByte = 0xAB;

    // vehicle.speed is float32; writing BYTE → float32 is INVALID per spec
    EXPECT_EQ(hsil_write_signal_by_name(h, kTopic, &sb.sample, "vehicle.speed", 0, val, HSIL_SIGNAL_BYTE),
              HSIL_ERR_SIGNAL_CONVERSION);

    hsil_destroy(h);
}

TEST_F(Validation_hsil_write_signal_by_name, ExactTypeWritesCorrectly)
{
    HsilHandle h = nullptr;
    ASSERT_EQ(hsil_create_from_config(kFixture, nullptr, &h), HSIL_OK);

    SampleBuffer sb;
    const float written = 99.0f;
    HsilSignalValue val{};
    val.asFloat32 = written;

    ASSERT_EQ(hsil_write_signal_by_name(h, kTopic, &sb.sample, "vehicle.speed", 0, val, HSIL_SIGNAL_FLOAT32),
              HSIL_OK);

    // Verify the bytes landed at offset 0
    float readBack = 0.0f;
    std::memcpy(&readBack, &sb.buf[0], sizeof(float));
    EXPECT_FLOAT_EQ(readBack, written);

    hsil_destroy(h);
}

TEST_F(Validation_hsil_write_signal_by_name, LossyConversionReturnsWarn)
{
    HsilHandle h = nullptr;
    ASSERT_EQ(hsil_create_from_config(kFixture, nullptr, &h), HSIL_OK);

    SampleBuffer sb;
    HsilSignalValue val{};
    val.asFloat64 = 3.14;

    // vehicle.speed is float32; writing float64 → float32 is LOSSY per spec
    int rc = hsil_write_signal_by_name(h, kTopic, &sb.sample, "vehicle.speed", 0, val, HSIL_SIGNAL_FLOAT64);
    EXPECT_EQ(rc, HSIL_WARN_SIGNAL_CONVERSION);

    // Despite the warning, the value was still written (memcpy of configured wire size)
    float readBack = 0.0f;
    std::memcpy(&readBack, &sb.buf[0], sizeof(float));
    EXPECT_NE(readBack, 0.0f);  // something was written

    hsil_destroy(h);
}

TEST_F(Validation_hsil_write_signal_by_name, Int32WrittenToGearPos)
{
    HsilHandle h = nullptr;
    ASSERT_EQ(hsil_create_from_config(kFixture, nullptr, &h), HSIL_OK);

    SampleBuffer sb;
    const int32_t gear = 3;
    HsilSignalValue val{};
    val.asInt32 = gear;

    ASSERT_EQ(hsil_write_signal_by_name(h, kTopic, &sb.sample, "vehicle.gearPos", 0, val, HSIL_SIGNAL_INT32),
              HSIL_OK);

    int32_t readBack = 0;
    std::memcpy(&readBack, &sb.buf[8], sizeof(int32_t)); // vehicle.gearPos is at offset 8
    EXPECT_EQ(readBack, gear);

    hsil_destroy(h);
}

TEST_F(Validation_hsil_write_signal_by_name, WritesEachArrayElementByIndex)
{
    HsilHandle h = nullptr;
    ASSERT_EQ(hsil_create_from_config(kFixture, nullptr, &h), HSIL_OK);

    SampleBuffer sb;
    const float expected[4] = { 15.0f, 25.0f, 35.0f, 45.0f };

    for (size_t i = 0; i < 4; ++i)
    {
        HsilSignalValue val{};
        val.asFloat32 = expected[i];
        ASSERT_EQ(hsil_write_signal_by_name(h, kTopic, &sb.sample, "vehicle.wheelSpeeds", i, val, HSIL_SIGNAL_FLOAT32), HSIL_OK)
            << "arrayIndex=" << i;
    }

    for (size_t i = 0; i < 4; ++i)
    {
        float readBack = 0.0f;
        std::memcpy(&readBack, &sb.buf[12 + i * sizeof(float)], sizeof(float));
        EXPECT_FLOAT_EQ(readBack, expected[i]) << "arrayIndex=" << i;
    }

    hsil_destroy(h);
}

TEST_F(Validation_hsil_write_signal_by_name, ArrayOutOfRangeReturnsGenericError)
{
    HsilHandle h = nullptr;
    ASSERT_EQ(hsil_create_from_config(kFixture, nullptr, &h), HSIL_OK);

    SampleBuffer sb;
    HsilSignalValue val{};
    val.asFloat32 = 0.0f;

    // vehicle.wheelSpeeds has count=4; index 4 is one past the end
    EXPECT_EQ(hsil_write_signal_by_name(h, kTopic, &sb.sample, "vehicle.wheelSpeeds", 4, val, HSIL_SIGNAL_FLOAT32),
              HSIL_ERR_GENERIC);

    hsil_destroy(h);
}

TEST_F(Validation_hsil_write_signal_by_name, WriteReadBack_SignalRoundTrip)
{
    HsilHandle h = nullptr;
    ASSERT_EQ(hsil_create_from_config(kFixture, nullptr, &h), HSIL_OK);

    SampleBuffer sb;
    const float rpm = 3500.0f;
    HsilSignalValue writeVal{};
    writeVal.asFloat32 = rpm;

    ASSERT_EQ(hsil_write_signal_by_name(h, kTopic, &sb.sample, "vehicle.rpm", 0, writeVal, HSIL_SIGNAL_FLOAT32),
              HSIL_OK);

    HsilSignalValue readVal{};
    HsilSignalType  readType{};
    ASSERT_EQ(hsil_read_signal_by_name(h, kTopic, &sb.sample, "vehicle.rpm", 0, &readVal, &readType),
              HSIL_OK);

    EXPECT_EQ(readType, HSIL_SIGNAL_FLOAT32);
    EXPECT_FLOAT_EQ(readVal.asFloat32, rpm);

    hsil_destroy(h);
}

TEST_F(Validation_hsil_write_signal_by_name, WriteReadBack_ArraySignalRoundTrip)
{
    HsilHandle h = nullptr;
    ASSERT_EQ(hsil_create_from_config(kFixture, nullptr, &h), HSIL_OK);

    SampleBuffer sb;
    const float writeValues[4] = { 100.0f, 200.0f, 300.0f, 400.0f };

    for (size_t i = 0; i < 4; ++i)
    {
        HsilSignalValue val{};
        val.asFloat32 = writeValues[i];
        ASSERT_EQ(hsil_write_signal_by_name(h, kTopic, &sb.sample, "vehicle.wheelSpeeds", i, val, HSIL_SIGNAL_FLOAT32), HSIL_OK);
    }

    for (size_t i = 0; i < 4; ++i)
    {
        HsilSignalValue readVal{};
        HsilSignalType  readType{};
        ASSERT_EQ(hsil_read_signal_by_name(h, kTopic, &sb.sample, "vehicle.wheelSpeeds", i, &readVal, &readType), HSIL_OK);
        EXPECT_EQ(readType, HSIL_SIGNAL_FLOAT32);
        EXPECT_FLOAT_EQ(readVal.asFloat32, writeValues[i]) << "arrayIndex=" << i;
    }

    hsil_destroy(h);
}

// ===========================================================================
// hsil_get_grouped_data_size
// ===========================================================================

static const char*    kSizeFixture = HSIL_TESTS_DIR "/api/fixtures/api_grouped_size.json";
static const uint32_t kSizeGroupId = 10u;  // group id in api_grouped_size.json

TEST_F(Validation_hsil_get_grouped_data_size, NullHandleReturnsInvalidArg)
{
    size_t sz = 0;
    EXPECT_EQ(hsil_get_grouped_data_size(nullptr, kGroupId, &sz), HSIL_ERR_INVALID_ARG);
}

TEST_F(Validation_hsil_get_grouped_data_size, NullDataSizeReturnsInvalidArg)
{
    HsilHandle h = nullptr;
    ASSERT_EQ(hsil_create_from_config(kFixture, nullptr, &h), HSIL_OK);

    EXPECT_EQ(hsil_get_grouped_data_size(h, kGroupId, nullptr), HSIL_ERR_INVALID_ARG);

    hsil_destroy(h);
}

TEST_F(Validation_hsil_get_grouped_data_size, NonConfigModeReturnsConfigError)
{
    HsilHandle h = nullptr;
    ASSERT_EQ(hsil_create(0, nullptr, &h), HSIL_OK);

    size_t sz = 0;
    EXPECT_EQ(hsil_get_grouped_data_size(h, kGroupId, &sz), HSIL_ERR_CONFIG);

    hsil_destroy(h);
}

TEST_F(Validation_hsil_get_grouped_data_size, UnknownGroupIdReturnsInvalidArg)
{
    HsilHandle h = nullptr;
    ASSERT_EQ(hsil_create_from_config(kFixture, nullptr, &h), HSIL_OK);

    size_t sz = 0;
    EXPECT_EQ(hsil_get_grouped_data_size(h, 9999u, &sz), HSIL_ERR_INVALID_ARG);

    hsil_destroy(h);
}

TEST_F(Validation_hsil_get_grouped_data_size, CorrectSizeAscendingOffsets)
{
    // api_signals.json: VehicleSignals group 1
    //   vehicle.speed        float32 offset  0 count 1  -> end  4
    //   vehicle.rpm          float32 offset  4 count 1  -> end  8
    //   vehicle.gearPos      int32   offset  8 count 1  -> end 12
    //   vehicle.wheelSpeeds  float32 offset 12 count 4  -> end 28
    // Expected: 28 bytes
    HsilHandle h = nullptr;
    ASSERT_EQ(hsil_create_from_config(kFixture, nullptr, &h), HSIL_OK);

    size_t sz = 0;
    ASSERT_EQ(hsil_get_grouped_data_size(h, kGroupId, &sz), HSIL_OK);
    EXPECT_EQ(sz, 28u);

    hsil_destroy(h);
}

TEST_F(Validation_hsil_get_grouped_data_size, CorrectSizeNonAscendingOffsets)
{
    // api_grouped_size.json: SizeTopic group 10 -- signals NOT in offset order
    //   sig.c  uint32  offset 8 count 1  -> end 12
    //   sig.a  float64 offset 0 count 1  -> end  8
    //   sig.b  uint8   offset 4 count 1  -> end  5
    // Expected: MAX(12, 8, 5) = 12 bytes
    HsilHandle h = nullptr;
    ASSERT_EQ(hsil_create_from_config(kSizeFixture, nullptr, &h), HSIL_OK);

    size_t sz = 0;
    ASSERT_EQ(hsil_get_grouped_data_size(h, kSizeGroupId, &sz), HSIL_OK);
    EXPECT_EQ(sz, 12u);

    hsil_destroy(h);
}
