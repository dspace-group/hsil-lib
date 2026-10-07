// SPDX-FileCopyrightText: 2026 dSPACE SE & Co. KG
// SPDX-License-Identifier: Apache-2.0

/** <!-------------------------------------------------------------------------->
*
*   @file hsilConfig.cpp
*
*   @brief Implementation of the HSIL JSON simulation configuration parser.
*
*   @author
*       dSPACE SE & Co. KG
*
*   @description
*       Reads an HSIL JSON simulation configuration file from disk and
*       populates the HsilSimulationConfig data structure. Uses nlohmann/json
*       for parsing. All errors are reported to stderr; no exceptions propagate
*       beyond this translation unit.
*
*   <hr><br>
*<!-------------------------------------------------------------------------->*/

/*----------------------------------------------------------------------------*/
/* INCLUDES                                                                   */
/*----------------------------------------------------------------------------*/

#include "hsilConfig.h"

#include <fstream>
#include <iostream>

#include <nlohmann/json.hpp>

/*----------------------------------------------------------------------------*/
/* NAMESPACE                                                                   */
/*----------------------------------------------------------------------------*/

namespace HSIL
{

/*----------------------------------------------------------------------------*/
/* INTERNAL HELPERS                                                            */
/*----------------------------------------------------------------------------*/

HsilSignalType parseDataType(const std::string& name)
{
    if (name == "bool")    return HsilSignalType::HSIL_SIGNAL_BOOL;
    if (name == "int8")    return HsilSignalType::HSIL_SIGNAL_INT8;
    if (name == "int16")   return HsilSignalType::HSIL_SIGNAL_INT16;
    if (name == "int32")   return HsilSignalType::HSIL_SIGNAL_INT32;
    if (name == "int64")   return HsilSignalType::HSIL_SIGNAL_INT64;
    if (name == "uint8")   return HsilSignalType::HSIL_SIGNAL_UINT8;
    if (name == "uint16")  return HsilSignalType::HSIL_SIGNAL_UINT16;
    if (name == "uint32")  return HsilSignalType::HSIL_SIGNAL_UINT32;
    if (name == "uint64")  return HsilSignalType::HSIL_SIGNAL_UINT64;
    if (name == "float32") return HsilSignalType::HSIL_SIGNAL_FLOAT32;
    if (name == "float64") return HsilSignalType::HSIL_SIGNAL_FLOAT64;
    if (name == "byte")    return HsilSignalType::HSIL_SIGNAL_BYTE;
    if (name == "char")    return HsilSignalType::HSIL_SIGNAL_CHAR;

    std::cerr << "[HSIL] Unknown data type: '" << name << "'. "
              << "Valid types: bool, int8, int16, int32, int64, uint8, uint16, "
              << "uint32, uint64, float32, float64, byte, char.\n";
    return HsilSignalType::HSIL_SIGNAL_UNKNOWN;
}

/** Parse and validate QoS overrides. Missing optional fields stay at defaults. */
static bool parseQos(const nlohmann::json& j, QosConfig& qos, const std::string& ctx)
{
    qos = QosConfig{};
    qos.queueLength = 0;

    if (j.contains("priority"))
    {
        if (!j["priority"].is_string())
        {
            std::cerr << "[HSIL] " << ctx << ": 'priority' must be a string.\n";
            return false;
        }
        const auto& p = j["priority"].get<std::string>();
        if (p != "normal" && p != "low" && p != "high")
        {
            std::cerr << "[HSIL] " << ctx << ": 'priority' must be 'normal', 'low', or 'high';"
                      << " got '" << p << "'.\n";
            return false;
        }
        qos.priority = p;
    }

    if (j.contains("reliability"))
    {
        if (!j["reliability"].is_string())
        {
            std::cerr << "[HSIL] " << ctx << ": 'reliability' must be a string.\n";
            return false;
        }
        const auto& r = j["reliability"].get<std::string>();
        if (r != "fast" && r != "reliable")
        {
            std::cerr << "[HSIL] " << ctx << ": 'reliability' must be 'fast' or 'reliable';"
                      << " got '" << r << "'.\n";
            return false;
        }
        qos.reliability = r;
    }

    if (j.contains("queueLength"))
    {
        if (!j["queueLength"].is_number_integer())
        {
            std::cerr << "[HSIL] " << ctx << ": 'queueLength' must be an integer.\n";
            return false;
        }
        const int q = j["queueLength"].get<int>();
        if (q < 1)
        {
            std::cerr << "[HSIL] " << ctx << ": 'queueLength' must be >= 1; got " << q << ".\n";
            return false;
        }
        qos.queueLength = q;
    }

    return true;
}

/** Parse and validate a single signal entry inside a GroupedData description. */
static bool parseSignal(const nlohmann::json& j, SignalConfig& s, const std::string& ctx)
{
    // accessPoint: required, non-empty string
    if (!j.contains("accessPoint") || !j["accessPoint"].is_string())
    {
        std::cerr << "[HSIL] " << ctx << ": 'accessPoint' is required and must be a string.\n";
        return false;
    }
    s.accessPoint = j["accessPoint"].get<std::string>();
    if (s.accessPoint.empty())
    {
        std::cerr << "[HSIL] " << ctx << ": 'accessPoint' must not be empty.\n";
        return false;
    }

    // offset: required integer >= 0
    if (!j.contains("offset") || !j["offset"].is_number_integer())
    {
        std::cerr << "[HSIL] " << ctx << ": 'offset' is required and must be an integer.\n";
        return false;
    }
    const int off = j["offset"].get<int>();
    if (off < 0)
    {
        std::cerr << "[HSIL] " << ctx << ": 'offset' must be >= 0; got " << off << ".\n";
        return false;
    }
    s.offset = off;

    // type: required string, must be a known primitive type
    if (!j.contains("type") || !j["type"].is_string())
    {
        std::cerr << "[HSIL] " << ctx << ": 'type' is required and must be a string.\n";
        return false;
    }
    s.type = parseDataType(j["type"].get<std::string>());
    if (s.type == HsilSignalType::HSIL_SIGNAL_UNKNOWN)
    {
        return false;   // parseDataType already logged the error
    }

    // count: optional integer >= 1; defaults to 1
    if (j.contains("count"))
    {
        if (!j["count"].is_number_integer())
        {
            std::cerr << "[HSIL] " << ctx << ": 'count' must be an integer.\n";
            return false;
        }
        const int c = j["count"].get<int>();
        if (c < 1)
        {
            std::cerr << "[HSIL] " << ctx << ": 'count' must be >= 1; got " << c << ".\n";
            return false;
        }
        s.count = c;
    }
    else
    {
        s.count = 1;
    }

    return true;
}

/** Parse and validate a GroupedData entry (id + signals array). */
static bool parseGroupedData(const nlohmann::json& j, GroupedDataConfig& g, const std::string& ctx)
{
    // id: required integer
    if (!j.contains("id") || !j["id"].is_number_integer())
    {
        std::cerr << "[HSIL] " << ctx << ": 'id' is required and must be an integer.\n";
        return false;
    }
    g.id = j["id"].get<uint32_t>();

    // signals: required array, minimum 1 item
    if (!j.contains("signals") || !j["signals"].is_array())
    {
        std::cerr << "[HSIL] " << ctx << " (id=" << g.id
                  << "): 'signals' is required and must be an array.\n";
        return false;
    }
    if (j["signals"].empty())
    {
        std::cerr << "[HSIL] " << ctx << " (id=" << g.id
                  << "): 'signals' must contain at least one entry.\n";
        return false;
    }

    for (size_t i = 0; i < j["signals"].size(); ++i)
    {
        if (!j["signals"][i].is_object())
        {
            std::cerr << "[HSIL] " << ctx << " (id=" << g.id
                      << "): signals[" << i << "] must be an object.\n";
            return false;
        }
        SignalConfig s;
        const std::string sigCtx = ctx + " signals[" + std::to_string(i) + "]";
        if (!parseSignal(j["signals"][i], s, sigCtx))
        {
            return false;
        }
        g.signals.push_back(std::move(s));
    }

    return true;
}

/** Parse and validate a StreamingData entry (protocol + accessPoint). */
static bool parseStreamingData(const nlohmann::json& j, StreamingDataConfig& s, const std::string& ctx)
{
    // protocol: required string, 1-8 printable ASCII characters [!-~] 
    // and one of ["generic", "CAN_v1", "Eth_v1", "EthJ_v1"]
    if (!j.contains("protocol") || !j["protocol"].is_string())
    {
        std::cerr << "[HSIL] " << ctx << ": 'protocol' is required and must be a string.\n";
        return false;
    }
    s.protocol = j["protocol"].get<std::string>();
    if (s.protocol.empty() || s.protocol.size() > 8)
    {
        std::cerr << "[HSIL] " << ctx << ": 'protocol' must be 1-8 characters; got '"
                  << s.protocol << "'.\n";
        return false;
    }
    for (char c : s.protocol)
    {
        if (c < '!' || c > '~')
        {
            std::cerr << "[HSIL] " << ctx
                      << ": 'protocol' must contain only printable ASCII (0x21-0x7E); got '"
                      << s.protocol << "'.\n";
            return false;
        }
    }
    if ((s.protocol != "CAN_v1")  && 
        (s.protocol != "Eth_v1")  &&
        (s.protocol != "generic") &&
        (s.protocol != "EthJ_v1"))
    {
        std::cerr << "[HSIL] " << ctx << ": 'protocol' must be one of [\"generic\", \"CAN_v1\", \"Eth_v1\", \"EthJ_v1\"]; got '"
                  << s.protocol << "'.\n";
        return false;
    }

    // accessPoint: required, non-empty string
    if (!j.contains("accessPoint") || !j["accessPoint"].is_string())
    {
        std::cerr << "[HSIL] " << ctx << ": 'accessPoint' is required and must be a string.\n";
        return false;
    }
    s.accessPoint = j["accessPoint"].get<std::string>();
    if (s.accessPoint.empty())
    {
        std::cerr << "[HSIL] " << ctx << ": 'accessPoint' must not be empty.\n";
        return false;
    }

    return true;
}

/**
 * @brief Heuristic to distinguish grouped from streaming entries.
 *
 * GroupedData entries contain an "id" field; StreamingData entries contain
 * "protocol". The HSIL schema guarantees the array is homogeneous, so
 * inspecting the first element is sufficient.
 */
static bool isGroupedDataArray(const nlohmann::json& arr)
{
    if (arr.empty() || !arr[0].is_object()) return false;
    return arr[0].contains("id");
}

/** Parse and validate the published/subscribed arrays of one topic. */
static bool parseTopicEntries(const nlohmann::json& arr, bool isPublished,
                              TopicConfig& topic, const std::string& ctx)
{
    if (isGroupedDataArray(arr))
    {
        for (size_t i = 0; i < arr.size(); ++i)
        {
            if (!arr[i].is_object())
            {
                std::cerr << "[HSIL] " << ctx << "[" << i << "] must be an object.\n";
                return false;
            }
            GroupedDataConfig g;
            const std::string entryCtx = ctx + "[" + std::to_string(i) + "]";
            if (!parseGroupedData(arr[i], g, entryCtx))
            {
                return false;
            }
            if (isPublished)
                topic.publishedGrouped.push_back(std::move(g));
            else
                topic.subscribedGrouped.push_back(std::move(g));
        }
    }
    else
    {
        for (size_t i = 0; i < arr.size(); ++i)
        {
            if (!arr[i].is_object())
            {
                std::cerr << "[HSIL] " << ctx << "[" << i << "] must be an object.\n";
                return false;
            }
            StreamingDataConfig s;
            const std::string entryCtx = ctx + "[" + std::to_string(i) + "]";
            if (!parseStreamingData(arr[i], s, entryCtx))
            {
                return false;
            }
            if (isPublished)
                topic.publishedStreaming.push_back(std::move(s));
            else
                topic.subscribedStreaming.push_back(std::move(s));
        }
    }

    return true;
}

/** Parse and validate a single topic object from the configuration JSON. */
static bool parseTopic(const nlohmann::json& j, TopicConfig& topic, const std::string& ctx)
{
    // name: required string, 1-256 characters
    if (!j.contains("name") || !j["name"].is_string())
    {
        std::cerr << "[HSIL] " << ctx << ": 'name' is required and must be a string.\n";
        return false;
    }
    topic.name = j["name"].get<std::string>();
    if (topic.name.empty())
    {
        std::cerr << "[HSIL] " << ctx << ": 'name' must not be empty.\n";
        return false;
    }
    if (topic.name.size() > 256)
    {
        std::cerr << "[HSIL] " << ctx << ": 'name' exceeds 256 characters (got "
                  << topic.name.size() << ").\n";
        return false;
    }

    const std::string topicId = ctx + " '" + topic.name + "'";

    // customKey: optional string
    if (j.contains("customKey"))
    {
        if (!j["customKey"].is_string())
        {
            std::cerr << "[HSIL] " << topicId << ": 'customKey' must be a string.\n";
            return false;
        }
        topic.customKey = j["customKey"].get<std::string>();
    }

    // qos: optional object with validated fields
    if (j.contains("qos"))
    {
        if (!j["qos"].is_object())
        {
            std::cerr << "[HSIL] " << topicId << ": 'qos' must be an object.\n";
            return false;
        }
        if (!parseQos(j["qos"], topic.qos, topicId + " qos"))
        {
            return false;
        }
    }

    // Must have at least one of 'published' or 'subscribed'
    const bool hasPublished  = j.contains("published")  && j["published"].is_array();
    const bool hasSubscribed = j.contains("subscribed") && j["subscribed"].is_array();

    if (!hasPublished && !hasSubscribed)
    {
        std::cerr << "[HSIL] " << topicId << ": must have 'published' and/or 'subscribed'.\n";
        return false;
    }

    if (hasPublished)
    {
        if (j["published"].empty())
        {
            std::cerr << "[HSIL] " << topicId
                      << ": 'published' must contain at least one entry.\n";
            return false;
        }
        if (!parseTopicEntries(j["published"], /*isPublished=*/true, topic,
                               topicId + " published"))
        {
            return false;
        }
    }

    if (hasSubscribed)
    {
        if (j["subscribed"].empty())
        {
            std::cerr << "[HSIL] " << topicId
                      << ": 'subscribed' must contain at least one entry.\n";
            return false;
        }
        if (!parseTopicEntries(j["subscribed"], /*isPublished=*/false, topic,
                               topicId + " subscribed"))
        {
            return false;
        }
    }

    // A topic cannot mix GroupedData and StreamingData
    const bool hasGrouped  = !topic.publishedGrouped.empty()   || !topic.subscribedGrouped.empty();
    const bool hasStreaming = !topic.publishedStreaming.empty() || !topic.subscribedStreaming.empty();
    if (hasGrouped && hasStreaming)
    {
        std::cerr << "[HSIL] " << topicId
                  << ": cannot mix GroupedData and StreamingData in the same topic.\n";
        return false;
    }

    return true;
}

/*----------------------------------------------------------------------------*/
/* PUBLIC FUNCTION                                                             */
/*----------------------------------------------------------------------------*/

bool parseSimulationConfig(const std::string& path, HsilSimulationConfig& config)
{
    std::ifstream file(path);
    if (!file.is_open())
    {
        std::cerr << "[HSIL] Cannot open configuration file: " << path << "\n";
        return false;
    }

    nlohmann::json j;
    try
    {
        file >> j;
    }
    catch (const nlohmann::json::parse_error& e)
    {
        std::cerr << "[HSIL] JSON parse error in '" << path << "': " << e.what() << "\n";
        return false;
    }

    // $hsil-simulation-configuration-version: optional integer
    if (j.contains("$hsil-simulation-configuration-version"))
    {
        if (!j["$hsil-simulation-configuration-version"].is_number_integer())
        {
            std::cerr << "[HSIL] '$hsil-simulation-configuration-version' must be an integer.\n";
            return false;
        }
        config.version = j["$hsil-simulation-configuration-version"].get<int>();
    }

    if (!j.contains("communication") || !j["communication"].is_object())
    {
        std::cerr << "[HSIL] Configuration file is missing the required 'communication' object.\n";
        return false;
    }

    const auto& comm = j["communication"];

    // domain: required integer in [0, 232]
    if (!comm.contains("domain") || !comm["domain"].is_number_integer())
    {
        std::cerr << "[HSIL] 'communication.domain' is required and must be an integer.\n";
        return false;
    }
    config.domain = comm["domain"].get<int>();
    if (config.domain < 0 || config.domain > 232)
    {
        std::cerr << "[HSIL] 'communication.domain' must be in the range [0, 232]; got "
                  << config.domain << ".\n";
        return false;
    }

    // topics: optional array (absent or empty = no DDS entities pre-created)
    if (comm.contains("topics"))
    {
        if (!comm["topics"].is_array())
        {
            std::cerr << "[HSIL] 'communication.topics' must be an array.\n";
            return false;
        }

        for (size_t i = 0; i < comm["topics"].size(); ++i)
        {
            if (!comm["topics"][i].is_object())
            {
                std::cerr << "[HSIL] topics[" << i << "] must be an object.\n";
                return false;
            }
            TopicConfig topic;
            const std::string topicCtx = "topics[" + std::to_string(i) + "]";
            if (!parseTopic(comm["topics"][i], topic, topicCtx))
            {
                return false;
            }
            config.topics.push_back(std::move(topic));
        }
    }

    return true;
}

} // namespace HSIL
