// SPDX-FileCopyrightText: 2026 dSPACE SE & Co. KG
// SPDX-License-Identifier: Apache-2.0

/** <!-------------------------------------------------------------------------->
*
*   @file test_hsilConfig.cpp
*
*   @brief Layer 1 HSIL configuration parser tests.
*
*   @author
*       dSPACE SE & Co. KG
*
*   @description
*       Tests parseDataType() and parseSimulationConfig() without a DDS
*       dependency. Test fixtures are stored in config/fixtures/.
*
*   <hr><br>
*<!-------------------------------------------------------------------------->*/

#include <gtest/gtest.h>

#include <hsil/hsil.h>
#include "hsilConfig.h"

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// HSIL_TESTS_DIR points to lib/tests/ (injected by CMake).
// Each layer stores its fixtures under a subdirectory named after the layer.
static std::string fixture(const char* name)
{
    return std::string(HSIL_TESTS_DIR) + "/config/fixtures/" + name;
}

// ===========================================================================
// parseDataType
// ===========================================================================

TEST(ParseDataType, KnownTypes)
{
    using HSIL::parseDataType;

    EXPECT_EQ(parseDataType("bool"),    HsilSignalType::HSIL_SIGNAL_BOOL);
    EXPECT_EQ(parseDataType("int8"),    HsilSignalType::HSIL_SIGNAL_INT8);
    EXPECT_EQ(parseDataType("int16"),   HsilSignalType::HSIL_SIGNAL_INT16);
    EXPECT_EQ(parseDataType("int32"),   HsilSignalType::HSIL_SIGNAL_INT32);
    EXPECT_EQ(parseDataType("int64"),   HsilSignalType::HSIL_SIGNAL_INT64);
    EXPECT_EQ(parseDataType("uint8"),   HsilSignalType::HSIL_SIGNAL_UINT8);
    EXPECT_EQ(parseDataType("uint16"),  HsilSignalType::HSIL_SIGNAL_UINT16);
    EXPECT_EQ(parseDataType("uint32"),  HsilSignalType::HSIL_SIGNAL_UINT32);
    EXPECT_EQ(parseDataType("uint64"),  HsilSignalType::HSIL_SIGNAL_UINT64);
    EXPECT_EQ(parseDataType("float32"), HsilSignalType::HSIL_SIGNAL_FLOAT32);
    EXPECT_EQ(parseDataType("float64"), HsilSignalType::HSIL_SIGNAL_FLOAT64);
    EXPECT_EQ(parseDataType("byte"),    HsilSignalType::HSIL_SIGNAL_BYTE);
    EXPECT_EQ(parseDataType("char"),    HsilSignalType::HSIL_SIGNAL_CHAR);
}

TEST(ParseDataType, UnknownTypeReturnsUnknown)
{
    EXPECT_EQ(HSIL::parseDataType("double"),    HsilSignalType::HSIL_SIGNAL_UNKNOWN);
    EXPECT_EQ(HSIL::parseDataType("INT32"),     HsilSignalType::HSIL_SIGNAL_UNKNOWN);
    EXPECT_EQ(HSIL::parseDataType(""),          HsilSignalType::HSIL_SIGNAL_UNKNOWN);
    EXPECT_EQ(HSIL::parseDataType("float"),     HsilSignalType::HSIL_SIGNAL_UNKNOWN);
}

// ===========================================================================
// parseSimulationConfig – file-not-found
// ===========================================================================

TEST(ParseSimulationConfig, NonExistentFileReturnsFalse)
{
    HSIL::HsilSimulationConfig cfg;
    EXPECT_FALSE(HSIL::parseSimulationConfig("/no/such/file.json", cfg));
}

// ===========================================================================
// parseSimulationConfig – malformed / schema violations
// ===========================================================================

TEST(ParseSimulationConfig, InvalidJsonReturnsFalse)
{
    HSIL::HsilSimulationConfig cfg;
    EXPECT_FALSE(HSIL::parseSimulationConfig(fixture("invalid_json.json"), cfg));
}

TEST(ParseSimulationConfig, MissingCommunicationObjectReturnsFalse)
{
    HSIL::HsilSimulationConfig cfg;
    EXPECT_FALSE(HSIL::parseSimulationConfig(fixture("missing_communication.json"), cfg));
}

TEST(ParseSimulationConfig, MissingDomainReturnsFalse)
{
    HSIL::HsilSimulationConfig cfg;
    EXPECT_FALSE(HSIL::parseSimulationConfig(fixture("missing_domain.json"), cfg));
}

// ===========================================================================
// parseSimulationConfig – valid minimal config
// ===========================================================================

TEST(ParseSimulationConfig, ValidMinimalConfig)
{
    HSIL::HsilSimulationConfig cfg;
    ASSERT_TRUE(HSIL::parseSimulationConfig(fixture("valid_minimal.json"), cfg));

    EXPECT_EQ(cfg.version, 2);
    EXPECT_EQ(cfg.domain,  10);
    EXPECT_TRUE(cfg.topics.empty());
}

// ===========================================================================
// parseSimulationConfig – full config with topics, signals and QoS
// ===========================================================================

TEST(ParseSimulationConfig, ValidFullConfig_TopicCount)
{
    HSIL::HsilSimulationConfig cfg;
    ASSERT_TRUE(HSIL::parseSimulationConfig(fixture("valid_full.json"), cfg));

    EXPECT_EQ(cfg.domain, 42);
    ASSERT_EQ(cfg.topics.size(), 2u);
}

TEST(ParseSimulationConfig, ValidFullConfig_GroupedTopic)
{
    HSIL::HsilSimulationConfig cfg;
    ASSERT_TRUE(HSIL::parseSimulationConfig(fixture("valid_full.json"), cfg));

    const HSIL::TopicConfig& t = cfg.topics[0];
    EXPECT_EQ(t.name, "VehicleSignals");

    // QoS
    EXPECT_EQ(t.qos.priority,    "high");
    EXPECT_EQ(t.qos.reliability, "reliable");
    EXPECT_EQ(t.qos.queueLength, 64);

    // Published grouped
    ASSERT_EQ(t.publishedGrouped.size(), 1u);
    EXPECT_EQ(t.publishedGrouped[0].id, 1u);
    ASSERT_EQ(t.publishedGrouped[0].signals.size(), 3u);

    EXPECT_EQ(t.publishedGrouped[0].signals[0].accessPoint, "vehicle.speed");
    EXPECT_EQ(t.publishedGrouped[0].signals[0].offset,       0);
    EXPECT_EQ(t.publishedGrouped[0].signals[0].type,         HsilSignalType::HSIL_SIGNAL_FLOAT32);

    EXPECT_EQ(t.publishedGrouped[0].signals[2].accessPoint, "vehicle.gearPos");
    EXPECT_EQ(t.publishedGrouped[0].signals[2].offset,       8);
    EXPECT_EQ(t.publishedGrouped[0].signals[2].type,         HsilSignalType::HSIL_SIGNAL_INT32);

    // No streaming or subscribed entries on this topic
    EXPECT_TRUE(t.publishedStreaming.empty());
    EXPECT_TRUE(t.subscribedGrouped.empty());
    EXPECT_TRUE(t.subscribedStreaming.empty());
}

TEST(ParseSimulationConfig, ValidFullConfig_StreamingTopic)
{
    HSIL::HsilSimulationConfig cfg;
    ASSERT_TRUE(HSIL::parseSimulationConfig(fixture("valid_full.json"), cfg));

    const HSIL::TopicConfig& t = cfg.topics[1];
    EXPECT_EQ(t.name, "CanBus0");

    EXPECT_EQ(t.qos.priority,    "normal");
    EXPECT_EQ(t.qos.reliability, "fast");
    EXPECT_EQ(t.qos.queueLength, 0);   // not specified → default zero from parser

    ASSERT_EQ(t.subscribedStreaming.size(), 1u);
    EXPECT_EQ(t.subscribedStreaming[0].protocol,    "CAN_v1");
    EXPECT_EQ(t.subscribedStreaming[0].accessPoint, "can0.rx");

    ASSERT_EQ(t.publishedStreaming.size(), 1u);
    EXPECT_EQ(t.publishedStreaming[0].protocol,    "CAN_v1");
    EXPECT_EQ(t.publishedStreaming[0].accessPoint, "can0.tx");
}

// ---------------------------------------------------------------------------
// Inline-JSON helper: writes content to the system temp dir and returns the
// path. Each test uses a unique stem so parallel invocations stay isolated.
// All created files are tracked in g_tempFiles and removed after the full
// test run by TempFileCleanup (registered as a global test environment).
// ---------------------------------------------------------------------------
static std::vector<std::string> g_tempFiles;

class TempFileCleanup : public ::testing::Environment
{
public:
    void TearDown() override
    {
        for (const auto& p : g_tempFiles)
            std::filesystem::remove(p);
        g_tempFiles.clear();
    }
};

// Registered once at program start; GoogleTest owns the pointer.
static const ::testing::Environment* const kCleanupEnv =
    ::testing::AddGlobalTestEnvironment(new TempFileCleanup);

static std::string inlineJson(const std::string& stem, const std::string& content)
{
    const std::string path =
        (std::filesystem::temp_directory_path() / ("hsil_cfgtest_" + stem + ".json")).string();
    std::ofstream(path, std::ios::trunc) << content;
    g_tempFiles.push_back(path);
    return path;
}

// ---------------------------------------------------------------------------
// Shared building blocks used by multiple validation tests.
// ---------------------------------------------------------------------------

// Wraps topicFields into a minimal valid config document.
static std::string topicJson(const std::string& topicFields)
{
    return R"({"communication":{"domain":10,"topics":[{)" + topicFields + R"(}]}})";
}

// Valid topic body (name + one published grouped entry with one signal).
static const std::string kValidTopicBody =
    R"("name":"T","published":[{"id":1,"signals":[{"accessPoint":"s","offset":0,"type":"float32"}]}])";

// A single valid signal object (as a JSON fragment, no surrounding braces).
static const std::string kSig =
    R"({"accessPoint":"s","offset":0,"type":"float32"})";

// Wraps a QoS value into a full config document.
static std::string qosTopicJson(const std::string& qosValue)
{
    return topicJson(kValidTopicBody + R"(,"qos":)" + qosValue);
}

// Wraps a grouped data entry into a full config document.
static std::string groupedTopicJson(const std::string& entry)
{
    return topicJson(R"("name":"T","published":[)" + entry + "]");
}

// Wraps signal fields into a full config document (one grouped entry).
static std::string signalTopicJson(const std::string& sigFields)
{
    return groupedTopicJson(R"({"id":1,"signals":[{)" + sigFields + "}]}");
}

// Wraps a streaming entry into a full config document.
static std::string streamingTopicJson(const std::string& entry)
{
    return topicJson(R"("name":"T","subscribed":[)" + entry + "]");
}

// ===========================================================================
// Validation: $hsil-simulation-configuration-version
// ===========================================================================

TEST(Validation_Version, NonIntegerReturnsFalse)
{
    HSIL::HsilSimulationConfig cfg;
    EXPECT_FALSE(HSIL::parseSimulationConfig(
        inlineJson("ver_nonint",
            R"({"$hsil-simulation-configuration-version":"two","communication":{"domain":10}})"),
        cfg));
}

TEST(Validation_Version, IntegerIsParsed)
{
    HSIL::HsilSimulationConfig cfg;
    ASSERT_TRUE(HSIL::parseSimulationConfig(
        inlineJson("ver_ok",
            R"({"$hsil-simulation-configuration-version":3,"communication":{"domain":10}})"),
        cfg));
    EXPECT_EQ(cfg.version, 3);
}

// ===========================================================================
// Validation: communication.domain
// ===========================================================================

TEST(Validation_Domain, NotIntegerReturnsFalse)
{
    HSIL::HsilSimulationConfig cfg;
    EXPECT_FALSE(HSIL::parseSimulationConfig(
        inlineJson("dom_str", R"({"communication":{"domain":"ten"}})"), cfg));
}

TEST(Validation_Domain, MinusOneReturnsFalse)
{
    HSIL::HsilSimulationConfig cfg;
    EXPECT_FALSE(HSIL::parseSimulationConfig(
        inlineJson("dom_neg", R"({"communication":{"domain":-1}})"), cfg));
}

TEST(Validation_Domain, AboveMaxReturnsFalse)
{
    HSIL::HsilSimulationConfig cfg;
    EXPECT_FALSE(HSIL::parseSimulationConfig(
        inlineJson("dom_hi", R"({"communication":{"domain":233}})"), cfg));
}

TEST(Validation_Domain, ZeroIsValid)
{
    HSIL::HsilSimulationConfig cfg;
    ASSERT_TRUE(HSIL::parseSimulationConfig(
        inlineJson("dom_zero", R"({"communication":{"domain":0}})"), cfg));
    EXPECT_EQ(cfg.domain, 0);
}

TEST(Validation_Domain, MaxValueIsValid)
{
    HSIL::HsilSimulationConfig cfg;
    ASSERT_TRUE(HSIL::parseSimulationConfig(
        inlineJson("dom_max", R"({"communication":{"domain":232}})"), cfg));
    EXPECT_EQ(cfg.domain, 232);
}

// ===========================================================================
// Validation: communication.topics array
// ===========================================================================

TEST(Validation_Topics, NotArrayReturnsFalse)
{
    HSIL::HsilSimulationConfig cfg;
    EXPECT_FALSE(HSIL::parseSimulationConfig(
        inlineJson("topics_obj",
            R"({"communication":{"domain":10,"topics":{}}})"),
        cfg));
}

TEST(Validation_Topics, ItemNotObjectReturnsFalse)
{
    HSIL::HsilSimulationConfig cfg;
    EXPECT_FALSE(HSIL::parseSimulationConfig(
        inlineJson("topics_stritem",
            R"({"communication":{"domain":10,"topics":["bad"]}})"),
        cfg));
}

// ===========================================================================
// Validation: topic.name
// ===========================================================================

TEST(Validation_TopicName, MissingReturnsFalse)
{
    HSIL::HsilSimulationConfig cfg;
    EXPECT_FALSE(HSIL::parseSimulationConfig(
        inlineJson("name_missing",
            R"({"communication":{"domain":10,"topics":[{"published":[{"id":1,"signals":[{"accessPoint":"s","offset":0,"type":"float32"}]}]}]}})"),
        cfg));
}

TEST(Validation_TopicName, NotStringReturnsFalse)
{
    HSIL::HsilSimulationConfig cfg;
    EXPECT_FALSE(HSIL::parseSimulationConfig(
        inlineJson("name_int",
            topicJson(R"("name":42,"published":[{"id":1,"signals":[{"accessPoint":"s","offset":0,"type":"float32"}]}])")),
        cfg));
}

TEST(Validation_TopicName, EmptyReturnsFalse)
{
    HSIL::HsilSimulationConfig cfg;
    EXPECT_FALSE(HSIL::parseSimulationConfig(
        inlineJson("name_empty",
            topicJson(R"("name":"","published":[{"id":1,"signals":[{"accessPoint":"s","offset":0,"type":"float32"}]}])")),
        cfg));
}

TEST(Validation_TopicName, TooLongReturnsFalse)
{
    const std::string longName(257, 'x');
    HSIL::HsilSimulationConfig cfg;
    EXPECT_FALSE(HSIL::parseSimulationConfig(
        inlineJson("name_long",
            R"({"communication":{"domain":10,"topics":[{"name":")" + longName +
            R"(","published":[{"id":1,"signals":[{"accessPoint":"s","offset":0,"type":"float32"}]}]}]}})"),
        cfg));
}

TEST(Validation_TopicName, Exactly256CharsIsValid)
{
    const std::string name256(256, 'x');
    HSIL::HsilSimulationConfig cfg;
    ASSERT_TRUE(HSIL::parseSimulationConfig(
        inlineJson("name_256",
            R"({"communication":{"domain":10,"topics":[{"name":")" + name256 +
            R"(","published":[{"id":1,"signals":[{"accessPoint":"s","offset":0,"type":"float32"}]}]}]}})"),
        cfg));
    EXPECT_EQ(cfg.topics[0].name, name256);
}

// ===========================================================================
// Validation: topic.customKey
// ===========================================================================

TEST(Validation_TopicCustomKey, NotStringReturnsFalse)
{
    HSIL::HsilSimulationConfig cfg;
    EXPECT_FALSE(HSIL::parseSimulationConfig(
        inlineJson("ck_int",
            topicJson(kValidTopicBody + R"(,"customKey":99)")),
        cfg));
}

TEST(Validation_TopicCustomKey, ValidStringIsParsed)
{
    HSIL::HsilSimulationConfig cfg;
    ASSERT_TRUE(HSIL::parseSimulationConfig(
        inlineJson("ck_ok",
            topicJson(kValidTopicBody + R"(,"customKey":"myKey")")),
        cfg));
    EXPECT_EQ(cfg.topics[0].customKey, "myKey");
}

// ===========================================================================
// Validation: topic.qos
// ===========================================================================

TEST(Validation_TopicQos, NotObjectReturnsFalse)
{
    HSIL::HsilSimulationConfig cfg;
    EXPECT_FALSE(HSIL::parseSimulationConfig(
        inlineJson("qos_arr", qosTopicJson("[]")), cfg));
}

TEST(Validation_TopicQos, PriorityNotStringReturnsFalse)
{
    HSIL::HsilSimulationConfig cfg;
    EXPECT_FALSE(HSIL::parseSimulationConfig(
        inlineJson("qos_prio_int", qosTopicJson(R"({"priority":1})")), cfg));
}

TEST(Validation_TopicQos, PriorityInvalidValueReturnsFalse)
{
    HSIL::HsilSimulationConfig cfg;
    EXPECT_FALSE(HSIL::parseSimulationConfig(
        inlineJson("qos_prio_bad", qosTopicJson(R"({"priority":"urgent"})")), cfg));
}

TEST(Validation_TopicQos, ReliabilityNotStringReturnsFalse)
{
    HSIL::HsilSimulationConfig cfg;
    EXPECT_FALSE(HSIL::parseSimulationConfig(
        inlineJson("qos_rel_bool", qosTopicJson(R"({"reliability":true})")), cfg));
}

TEST(Validation_TopicQos, ReliabilityInvalidValueReturnsFalse)
{
    HSIL::HsilSimulationConfig cfg;
    EXPECT_FALSE(HSIL::parseSimulationConfig(
        inlineJson("qos_rel_bad", qosTopicJson(R"({"reliability":"best-effort"})")), cfg));
}

TEST(Validation_TopicQos, QueueLengthNotIntegerReturnsFalse)
{
    HSIL::HsilSimulationConfig cfg;
    EXPECT_FALSE(HSIL::parseSimulationConfig(
        inlineJson("qos_ql_str", qosTopicJson(R"({"queueLength":"10"})")), cfg));
}

TEST(Validation_TopicQos, QueueLengthZeroReturnsFalse)
{
    HSIL::HsilSimulationConfig cfg;
    EXPECT_FALSE(HSIL::parseSimulationConfig(
        inlineJson("qos_ql_zero", qosTopicJson(R"({"queueLength":0})")), cfg));
}

TEST(Validation_TopicQos, QueueLengthNegativeReturnsFalse)
{
    HSIL::HsilSimulationConfig cfg;
    EXPECT_FALSE(HSIL::parseSimulationConfig(
        inlineJson("qos_ql_neg", qosTopicJson(R"({"queueLength":-5})")), cfg));
}

TEST(Validation_TopicQos, AllFieldsValidAreParsed)
{
    HSIL::HsilSimulationConfig cfg;
    ASSERT_TRUE(HSIL::parseSimulationConfig(
        inlineJson("qos_all_ok",
            qosTopicJson(R"({"priority":"low","reliability":"reliable","queueLength":8})")),
        cfg));
    EXPECT_EQ(cfg.topics[0].qos.priority,    "low");
    EXPECT_EQ(cfg.topics[0].qos.reliability, "reliable");
    EXPECT_EQ(cfg.topics[0].qos.queueLength, 8);
}

// ===========================================================================
// Validation: topic published / subscribed
// ===========================================================================

TEST(Validation_TopicDirection, NeitherPublishedNorSubscribedReturnsFalse)
{
    HSIL::HsilSimulationConfig cfg;
    EXPECT_FALSE(HSIL::parseSimulationConfig(
        inlineJson("dir_none", topicJson(R"("name":"T")")), cfg));
}

TEST(Validation_TopicDirection, PublishedEmptyReturnsFalse)
{
    HSIL::HsilSimulationConfig cfg;
    EXPECT_FALSE(HSIL::parseSimulationConfig(
        inlineJson("pub_empty", topicJson(R"("name":"T","published":[])")), cfg));
}

TEST(Validation_TopicDirection, SubscribedEmptyReturnsFalse)
{
    HSIL::HsilSimulationConfig cfg;
    EXPECT_FALSE(HSIL::parseSimulationConfig(
        inlineJson("sub_empty", topicJson(R"("name":"T","subscribed":[])")), cfg));
}

TEST(Validation_TopicDirection, BothPubAndSubGroupedAreParsed)
{
    const std::string grouped = R"([{"id":1,"signals":[)" + kSig + R"(]}])";
    HSIL::HsilSimulationConfig cfg;
    ASSERT_TRUE(HSIL::parseSimulationConfig(
        inlineJson("both_pub_sub",
            topicJson(R"("name":"T","published":)" + grouped +
                      R"(,"subscribed":)" + grouped)),
        cfg));
    EXPECT_EQ(cfg.topics[0].publishedGrouped.size(),  1u);
    EXPECT_EQ(cfg.topics[0].subscribedGrouped.size(), 1u);
}

// ===========================================================================
// Validation: GroupedData
// ===========================================================================

TEST(Validation_GroupedData, IdMissingReturnsFalse)
{
    HSIL::HsilSimulationConfig cfg;
    EXPECT_FALSE(HSIL::parseSimulationConfig(
        inlineJson("gd_id_miss",
            groupedTopicJson(R"({"signals":[)" + kSig + "]}")),
        cfg));
}

TEST(Validation_GroupedData, IdNotIntegerReturnsFalse)
{
    HSIL::HsilSimulationConfig cfg;
    EXPECT_FALSE(HSIL::parseSimulationConfig(
        inlineJson("gd_id_str",
            groupedTopicJson(R"({"id":"one","signals":[)" + kSig + "]}")),
        cfg));
}

TEST(Validation_GroupedData, SignalsMissingReturnsFalse)
{
    HSIL::HsilSimulationConfig cfg;
    EXPECT_FALSE(HSIL::parseSimulationConfig(
        inlineJson("gd_sig_miss", groupedTopicJson(R"({"id":1})")), cfg));
}

TEST(Validation_GroupedData, SignalsNotArrayReturnsFalse)
{
    HSIL::HsilSimulationConfig cfg;
    EXPECT_FALSE(HSIL::parseSimulationConfig(
        inlineJson("gd_sig_obj", groupedTopicJson(R"({"id":1,"signals":{}})")), cfg));
}

TEST(Validation_GroupedData, SignalsEmptyReturnsFalse)
{
    HSIL::HsilSimulationConfig cfg;
    EXPECT_FALSE(HSIL::parseSimulationConfig(
        inlineJson("gd_sig_empty", groupedTopicJson(R"({"id":1,"signals":[]})")), cfg));
}

TEST(Validation_GroupedData, SignalNotObjectReturnsFalse)
{
    HSIL::HsilSimulationConfig cfg;
    EXPECT_FALSE(HSIL::parseSimulationConfig(
        inlineJson("gd_sig_stritem",
            groupedTopicJson(R"({"id":1,"signals":["bad"]})")),
        cfg));
}

// ===========================================================================
// Validation: Signal
// ===========================================================================

TEST(Validation_Signal, AccessPointMissingReturnsFalse)
{
    HSIL::HsilSimulationConfig cfg;
    EXPECT_FALSE(HSIL::parseSimulationConfig(
        inlineJson("sig_ap_miss",
            signalTopicJson(R"("offset":0,"type":"float32")")), cfg));
}

TEST(Validation_Signal, AccessPointNotStringReturnsFalse)
{
    HSIL::HsilSimulationConfig cfg;
    EXPECT_FALSE(HSIL::parseSimulationConfig(
        inlineJson("sig_ap_int",
            signalTopicJson(R"("accessPoint":42,"offset":0,"type":"float32")")), cfg));
}

TEST(Validation_Signal, AccessPointEmptyReturnsFalse)
{
    HSIL::HsilSimulationConfig cfg;
    EXPECT_FALSE(HSIL::parseSimulationConfig(
        inlineJson("sig_ap_empty",
            signalTopicJson(R"("accessPoint":"","offset":0,"type":"float32")")), cfg));
}

TEST(Validation_Signal, OffsetMissingReturnsFalse)
{
    HSIL::HsilSimulationConfig cfg;
    EXPECT_FALSE(HSIL::parseSimulationConfig(
        inlineJson("sig_off_miss",
            signalTopicJson(R"("accessPoint":"s","type":"float32")")), cfg));
}

TEST(Validation_Signal, OffsetNotIntegerReturnsFalse)
{
    HSIL::HsilSimulationConfig cfg;
    EXPECT_FALSE(HSIL::parseSimulationConfig(
        inlineJson("sig_off_str",
            signalTopicJson(R"("accessPoint":"s","offset":"0","type":"float32")")), cfg));
}

TEST(Validation_Signal, OffsetNegativeReturnsFalse)
{
    HSIL::HsilSimulationConfig cfg;
    EXPECT_FALSE(HSIL::parseSimulationConfig(
        inlineJson("sig_off_neg",
            signalTopicJson(R"("accessPoint":"s","offset":-1,"type":"float32")")), cfg));
}

TEST(Validation_Signal, OffsetZeroIsValid)
{
    HSIL::HsilSimulationConfig cfg;
    ASSERT_TRUE(HSIL::parseSimulationConfig(
        inlineJson("sig_off_zero",
            signalTopicJson(R"("accessPoint":"s","offset":0,"type":"float32")")), cfg));
    EXPECT_EQ(cfg.topics[0].publishedGrouped[0].signals[0].offset, 0);
}

TEST(Validation_Signal, TypeMissingReturnsFalse)
{
    HSIL::HsilSimulationConfig cfg;
    EXPECT_FALSE(HSIL::parseSimulationConfig(
        inlineJson("sig_type_miss",
            signalTopicJson(R"("accessPoint":"s","offset":0)")), cfg));
}

TEST(Validation_Signal, TypeNotStringReturnsFalse)
{
    HSIL::HsilSimulationConfig cfg;
    EXPECT_FALSE(HSIL::parseSimulationConfig(
        inlineJson("sig_type_int",
            signalTopicJson(R"("accessPoint":"s","offset":0,"type":32)")), cfg));
}

TEST(Validation_Signal, TypeUnknownReturnsFalse)
{
    HSIL::HsilSimulationConfig cfg;
    EXPECT_FALSE(HSIL::parseSimulationConfig(
        inlineJson("sig_type_bad",
            signalTopicJson(R"("accessPoint":"s","offset":0,"type":"double")")), cfg));
}

TEST(Validation_Signal, CountNotIntegerReturnsFalse)
{
    HSIL::HsilSimulationConfig cfg;
    EXPECT_FALSE(HSIL::parseSimulationConfig(
        inlineJson("sig_cnt_str",
            signalTopicJson(R"("accessPoint":"s","offset":0,"type":"float32","count":"3")")), cfg));
}

TEST(Validation_Signal, CountZeroReturnsFalse)
{
    HSIL::HsilSimulationConfig cfg;
    EXPECT_FALSE(HSIL::parseSimulationConfig(
        inlineJson("sig_cnt_zero",
            signalTopicJson(R"("accessPoint":"s","offset":0,"type":"float32","count":0)")), cfg));
}

TEST(Validation_Signal, CountNegativeReturnsFalse)
{
    HSIL::HsilSimulationConfig cfg;
    EXPECT_FALSE(HSIL::parseSimulationConfig(
        inlineJson("sig_cnt_neg",
            signalTopicJson(R"("accessPoint":"s","offset":0,"type":"float32","count":-1)")), cfg));
}

TEST(Validation_Signal, CountValidIsParsed)
{
    HSIL::HsilSimulationConfig cfg;
    ASSERT_TRUE(HSIL::parseSimulationConfig(
        inlineJson("sig_cnt_ok",
            signalTopicJson(R"("accessPoint":"s","offset":0,"type":"uint8","count":4)")), cfg));
    EXPECT_EQ(cfg.topics[0].publishedGrouped[0].signals[0].count, 4);
    EXPECT_EQ(cfg.topics[0].publishedGrouped[0].signals[0].type, HsilSignalType::HSIL_SIGNAL_UINT8);
}

// ===========================================================================
// Validation: StreamingData
// ===========================================================================

TEST(Validation_StreamingData, ProtocolMissingReturnsFalse)
{
    HSIL::HsilSimulationConfig cfg;
    EXPECT_FALSE(HSIL::parseSimulationConfig(
        inlineJson("sd_proto_miss",
            streamingTopicJson(R"({"accessPoint":"a"})")), cfg));
}

TEST(Validation_StreamingData, ProtocolNotStringReturnsFalse)
{
    HSIL::HsilSimulationConfig cfg;
    EXPECT_FALSE(HSIL::parseSimulationConfig(
        inlineJson("sd_proto_int",
            streamingTopicJson(R"({"protocol":1,"accessPoint":"a"})")), cfg));
}

TEST(Validation_StreamingData, ProtocolEmptyReturnsFalse)
{
    HSIL::HsilSimulationConfig cfg;
    EXPECT_FALSE(HSIL::parseSimulationConfig(
        inlineJson("sd_proto_empty",
            streamingTopicJson(R"({"protocol":"","accessPoint":"a"})")), cfg));
}

TEST(Validation_StreamingData, ProtocolTooLongReturnsFalse)
{
    // "NINECHAR9" = 9 chars, exceeds the 8-char limit
    HSIL::HsilSimulationConfig cfg;
    EXPECT_FALSE(HSIL::parseSimulationConfig(
        inlineJson("sd_proto_long",
            streamingTopicJson(R"({"protocol":"NINECHAR9","accessPoint":"a"})")), cfg));
}

TEST(Validation_StreamingData, ProtocolWithSpaceReturnsFalse)
{
    HSIL::HsilSimulationConfig cfg;
    EXPECT_FALSE(HSIL::parseSimulationConfig(
        inlineJson("sd_proto_space",
            streamingTopicJson(R"({"protocol":"CA N","accessPoint":"a"})")), cfg));
}

TEST(Validation_StreamingData, AccessPointMissingReturnsFalse)
{
    HSIL::HsilSimulationConfig cfg;
    EXPECT_FALSE(HSIL::parseSimulationConfig(
        inlineJson("sd_ap_miss",
            streamingTopicJson(R"({"protocol":"CAN"})")), cfg));
}

TEST(Validation_StreamingData, AccessPointNotStringReturnsFalse)
{
    HSIL::HsilSimulationConfig cfg;
    EXPECT_FALSE(HSIL::parseSimulationConfig(
        inlineJson("sd_ap_int",
            streamingTopicJson(R"({"protocol":"CAN","accessPoint":0})")), cfg));
}

TEST(Validation_StreamingData, AccessPointEmptyReturnsFalse)
{
    HSIL::HsilSimulationConfig cfg;
    EXPECT_FALSE(HSIL::parseSimulationConfig(
        inlineJson("sd_ap_empty",
            streamingTopicJson(R"({"protocol":"CAN","accessPoint":""})")), cfg));
}

TEST(Validation_StreamingData, ValidIsParsed)
{
    HSIL::HsilSimulationConfig cfg;
    ASSERT_TRUE(HSIL::parseSimulationConfig(
        inlineJson("sd_ok",
            streamingTopicJson(R"({"protocol":"CAN_v1","accessPoint":"can.rx"})")), cfg));
    EXPECT_EQ(cfg.topics[0].subscribedStreaming[0].protocol,    "CAN_v1");
    EXPECT_EQ(cfg.topics[0].subscribedStreaming[0].accessPoint, "can.rx");
}

// ===========================================================================
// Validation: topic cannot mix GroupedData and StreamingData
// ===========================================================================

TEST(Validation_MixedTopicType, MixGroupedAndStreamingReturnsFalse)
{
    // published = grouped, subscribed = streaming
    const std::string json = topicJson(
        R"("name":"T",)"
        R"("published":[{"id":1,"signals":[)" + kSig + R"(]}],)"
        R"("subscribed":[{"protocol":"CAN","accessPoint":"a"}])");
    HSIL::HsilSimulationConfig cfg;
    EXPECT_FALSE(HSIL::parseSimulationConfig(inlineJson("mix_types", json), cfg));
}
