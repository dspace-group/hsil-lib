// SPDX-FileCopyrightText: 2026 dSPACE SE & Co. KG
// SPDX-License-Identifier: Apache-2.0

/** <!-------------------------------------------------------------------------->
*
*   @file hsil.cpp
*
*   @brief Implementation of the public HSIL CoSim C API.
*
*   @author
*       dSPACE SE & Co. KG
*
*   @description
*       Implements all functions declared in hsil.h. Acts as the bridge between
*       the public C API and the internal DDS abstraction layer (HsilDdsOps).
*       Manages session lifetime, subscriber dispatch, and config-mode setup.
*
*   <hr><br>
*<!-------------------------------------------------------------------------->*/

/*----------------------------------------------------------------------------*/
/* INCLUDES                                                                   */
/*----------------------------------------------------------------------------*/

#include <hsil/hsil.h>
#include "ddsAbstraction.h"
#include "hsilConfig.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

/*----------------------------------------------------------------------------*/
/* FORWARD DECLARATIONS                                                        */
/*----------------------------------------------------------------------------*/

struct HsilContext_s;
static void buildSignalIndex(HsilContext_s& ctx);
static size_t signalTypeSize(HsilSignalType type);

/*----------------------------------------------------------------------------*/
/* INTERNAL DATA STRUCTURES                                                    */
/*----------------------------------------------------------------------------*/

/**
 * @brief A single registered GroupedData callback with its filter criteria.
 */
struct GroupedCallbackEntry
{
    uint32_t                groupId;   /**< Filter id; HSIL_ALL_IDS means accept all. */
    HsilGroupedDataCallback callback;  /**< User-supplied callback.                   */
    void*                   userData;  /**< Opaque context forwarded to callback.      */
};

/**
 * @brief A single registered StreamingData callback with its filter criteria.
 */
struct StreamingCallbackEntry
{
    std::string               protocol; /**< Empty string means accept all protocols. */
    HsilStreamingDataCallback callback; /**< User-supplied callback.                  */
    void*                     userData; /**< Opaque context forwarded to callback.     */
};

/**
 * @brief The concrete type behind the opaque HsilSubscriber handle.
 *
 * Wraps a single DDS reader for one topic. The vendor layer creates the reader
 * and calls the matching static dispatch function for every arriving sample,
 * passing this object as the callback data. The dispatch function iterates the
 * callback entry vectors and forwards the sample to any matching user callback.
 *
 * Exactly one HsilSubscriber_s exists per subscribed topic; it is owned by the
 * topic's TopicState (and therefore by the session).
 */
struct HsilSubscriber_s
{
    const HsilDdsOps* ops;          /**< DDS ops vtable.                */
    void*             ddsCtx;       /**< Vendor context.                */
    void*             readerHandle; /**< Vendor-specific reader handle. */
    HsilTopicType     type;         /**< Grouped or streaming.          */

    /** Protects the callback entry vectors from concurrent modification. */
    std::mutex  entriesMutex;

    std::vector<GroupedCallbackEntry>   groupedEntries;
    std::vector<StreamingCallbackEntry> streamingEntries;
};

/**
 * @brief The concrete type behind the opaque HsilPublisher handle.
 *
 * Wraps a single DDS writer for one topic. Exactly one HsilPublisher_s exists
 * per published topic; it is owned by the topic's TopicState (and therefore by
 * the session). Users never free it.
 */
struct HsilPublisher_s
{
    const HsilDdsOps* ops;          /**< DDS ops vtable.                */
    void*             ddsCtx;       /**< Vendor context.                */
    void*             writerHandle; /**< Vendor-specific writer handle. */
    HsilTopicType     type;         /**< Grouped or streaming.          */
};

/**
 * @brief Per-topic runtime state, keyed by topic name in HsilContext_s.
 *
 * A topic carries either GroupedData or StreamingData (never both); @ref type
 * records which. A topic may be both subscribed and published, so it can hold a
 * reader (@ref subscriber) and/or a writer (@ref publisher) at the same time.
 */
struct TopicState
{
    std::string                       topicName;  /**< DDS topic name.                 */
    HsilTopicType                     type;       /**< Grouped or streaming.           */
    std::unique_ptr<HsilSubscriber_s> subscriber; /**< Reader state; null if none.     */
    std::unique_ptr<HsilPublisher_s>  publisher;  /**< Writer state; null if none.     */
};

/**
 * @brief The concrete type behind the opaque HsilHandle.
 */
struct HsilContext_s
{
    const HsilDdsOps* ops;        /**< DDS ops vtable.      */
    void*             ddsCtx;     /**< Vendor context.      */
    bool              configMode; /**< true if created from config. */

    /** Parsed simulation configuration; populated in config mode only. */
    HSIL::HsilSimulationConfig simulationConfig;

    /**
     * @brief Fast topic lookup built from simulationConfig at config-load time.
     *
     * Key: topic name. Pointers remain valid for the lifetime of simulationConfig.
     */
    std::unordered_map<std::string, const HSIL::TopicConfig*> topicIndex;

    /**
     * @brief Fast signal lookup built from simulationConfig at config-load time.
     *
     * Key: makeSignalKey(topicName, groupId, signalName).
     * Subscribed signals take precedence over published signals for the same key.
     * Pointers remain valid for the lifetime of simulationConfig.
     */
    std::unordered_map<std::string, const HSIL::SignalConfig*> signalIndex;

    /**
     * @brief Pre-computed GroupedData payload sizes, keyed by group id.
     *
     * Key: groupId (globally unique per configuration).
     * Value: required byte count for HsilGroupedData.data.
     * Populated during buildSignalIndex(); subscribed groups take precedence.
     */
    std::unordered_map<uint32_t, size_t> groupSizeCache;

    /**
     * @brief topicName → per-topic reader/writer state.
     *
     * The single source of truth for every reader and writer the session owns.
     */
    std::map<std::string, std::unique_ptr<TopicState>> topicStates;
};

/*----------------------------------------------------------------------------*/
/* INTERNAL DISPATCH CALLBACKS                                                 */
/*----------------------------------------------------------------------------*/

/**
 * @brief Static GroupedData dispatch callback passed to the vendor reader.
 *
 * Receives every GroupedData sample for a topic and forwards it to all
 * registered user callbacks whose groupId filter matches.
 */
static void groupedDispatchCb(const char*            topicName,
                               const HsilGroupedData* data,
                               void*                  statePtr)
{
    auto* sub = static_cast<HsilSubscriber_s*>(statePtr);

    std::lock_guard<std::mutex> lock(sub->entriesMutex);
    for (const auto& entry : sub->groupedEntries)
    {
        if (entry.groupId == HSIL_ALL_IDS || entry.groupId == data->id)
        {
            entry.callback(topicName, data, entry.userData);
        }
    }
}

/**
 * @brief Static StreamingData dispatch callback passed to the vendor reader.
 *
 * Forwards streaming samples to user callbacks whose protocol filter matches.
 */
static void streamingDispatchCb(const char*              topicName,
                                 const HsilStreamingData* data,
                                 void*                    statePtr)
{
    auto* sub = static_cast<HsilSubscriber_s*>(statePtr);

    std::lock_guard<std::mutex> lock(sub->entriesMutex);
    for (const auto& entry : sub->streamingEntries)
    {
        /* An empty protocol string in the entry means "accept all". */
        const bool protocolMatch = entry.protocol.empty() ||
                                   (strncmp(entry.protocol.c_str(), data->protocol, 8) == 0);
        if (protocolMatch)
        {
            entry.callback(topicName, data, entry.userData);
        }
    }
}


/*----------------------------------------------------------------------------*/
/* INTERNAL HELPER FUNCTIONS                                                   */
/*----------------------------------------------------------------------------*/

/**
 * @brief Validate a caller-supplied QoS struct, replacing invalid fields with
 *        defaults and emitting a warning message for any replacement made.
 *
 * @param qos      Pointer to the caller's QoS; may be NULL.
 * @param context  Short description of the call site used in warning messages.
 * @return A fully populated, valid HsilQos struct.
 */
static HsilQos validateOrDefaultQos(const HsilQos* qos, const char* context)
{
    static const HsilQos kDefault = HSIL_DEFAULT_QOS;

    /* Use default QoS settings if no qos is supplied .*/
    if (qos == nullptr)
    {
        std::cerr << "[HSIL] " << context << ": NULL QoS supplied; using default QoS.\n";
        return kDefault;
    }

    HsilQos result = *qos;

    /* Priority: Use user supplied value if valid, otherwise default to NORMAL. */
    switch (result.priority)
    {
        case HSIL_QOS_PRIORITY_LOW:    
        case HSIL_QOS_PRIORITY_NORMAL:    
        case HSIL_QOS_PRIORITY_HIGH:
        {
            break; /* Valid values, do nothing. */
        }
        default:
        {
            std::cerr << "[HSIL] " << context << ": invalid QoS priority ("
                      << static_cast<int>(result.priority) << "); using NORMAL.\n";
            result.priority = HSIL_QOS_PRIORITY_NORMAL;
            break;
        }      
    }

    /* Queue length: Use user supplied value if valid, otherwise default to 1. */
    if (result.queueLength < 1)
    {
        std::cerr << "[HSIL] " << context << ": invalid QoS queueLength ("
                  << result.queueLength << "); using 1.\n";
        result.queueLength = 1;
    }

    /* Reliability: Use user supplied value if valid, otherwise default to FAST. */
    switch (result.reliability)
    {
        case HSIL_QOS_RELIABILITY_FAST:
        case HSIL_QOS_RELIABILITY_RELIABLE:
        {
            break; /* Valid values, do nothing. */
        }
        default:
        {
            std::cerr << "[HSIL] " << context << ": invalid QoS reliability ("
                      << static_cast<int>(result.reliability) << "); using FAST.\n";
            result.reliability = HSIL_QOS_RELIABILITY_FAST;
            break;
        }
    }

    return result;
}

/**
 * @brief Convert a parsed QosConfig (from the simulation config file) to a
 *        validated HsilQos struct.
 *
 * Unknown or missing string values fall back to the corresponding defaults and
 * a warning is emitted.
 */
static HsilQos qosFromConfig(const HSIL::QosConfig& cfg)
{
    static const HsilQos kDefault = HSIL_DEFAULT_QOS;
    HsilQos qos = kDefault;

    if (cfg.priority == "high")
    {
        qos.priority = HSIL_QOS_PRIORITY_HIGH;
    }
    else if (cfg.priority == "low")
    {
        qos.priority = HSIL_QOS_PRIORITY_LOW;
    }
    else if (!cfg.priority.empty() && cfg.priority != "normal")
    {
        std::cerr << "[HSIL] Config: unknown QoS priority '" << cfg.priority
                  << "'; using normal.\n";
    }

    if (cfg.queueLength > 0)
    {
        qos.queueLength = cfg.queueLength;
    }
    else if (cfg.queueLength < 0)
    {
        std::cerr << "[HSIL] Config: invalid QoS queueLength " << cfg.queueLength
                  << "; using 1.\n";
    }

    if (cfg.reliability == "reliable")
    {
        qos.reliability = HSIL_QOS_RELIABILITY_RELIABLE;
    }
    else if (!cfg.reliability.empty() && cfg.reliability != "fast")
    {
        std::cerr << "[HSIL] Config: unknown QoS reliability '" << cfg.reliability
                  << "'; using fast.\n";
    }

    return qos;
}

/**
 * @brief Normalise a caller-supplied protocol filter to the stored form.
 *
 * NULL becomes the empty string ("accept all"); otherwise the first up-to-8
 * characters are kept.
 */
static std::string normalizeProtocol(const char* protocol)
{
    /* strnlen is a POSIX extension unavailable on MSVC; use the portable equivalent. */
    return (protocol != nullptr)
        ? std::string(protocol, std::min(std::strlen(protocol), static_cast<size_t>(8)))
        : std::string();
}

/**
 * @brief Validate a caller-supplied protocol string, which must be one of
 *        ["generic", "CAN_v1", "Eth_v1", "EthJ_v1"]
 */
static bool isValidProtocol(const char* protocol)
{
    if (protocol == nullptr)
    {
        return false;
    }

    if (strcmp(protocol, "CAN_v1")  != 0  && 
        strcmp(protocol, "Eth_v1")  != 0 &&
        strcmp(protocol, "generic") != 0 &&
        strcmp(protocol, "EthJ_v1") != 0)
    {
        std::cerr << "[HSIL] 'protocol' must be one of [\"generic\", \"CAN_v1\", \"Eth_v1\", \"EthJ_v1\"]; got '"
                  << protocol << "'.\n";
        
        return false;
    }

    return true;
}

/**
 * @brief Find the TopicState for a topic, creating an empty one if absent.
 *
 * Establishes the topic's data type on first use. If the topic already exists
 * with a different type, sets @p err to HSIL_ERR_TOPIC_TYPE_MISMATCH and
 * returns nullptr.
 *
 * @param ctx        Session context.
 * @param topicName  DDS topic name.
 * @param type       Data type the caller intends to use the topic as.
 * @param[out] err   HSIL_OK on success or HSIL_ERR_TOPIC_TYPE_MISMATCH.
 * @return The TopicState, or nullptr on a type mismatch.
 */
static TopicState* getOrCreateTopicState(HsilContext_s* ctx,
                                         const char*    topicName,
                                         HsilTopicType  type,
                                         int&           err)
{
    err = HSIL_OK;

    auto it = ctx->topicStates.find(topicName);
    if (it != ctx->topicStates.end())
    {
        if (it->second->type != type)
        {
            std::cerr << "[HSIL] Topic '" << topicName << "' is already in use as a "
                      << (it->second->type == HSIL_TOPIC_GROUPED ? "grouped" : "streaming")
                      << " topic; cannot reuse it as "
                      << (type == HSIL_TOPIC_GROUPED ? "grouped" : "streaming") << " topic.\n";
            err = HSIL_ERR_TOPIC_TYPE_MISMATCH;
            return nullptr;
        }
        return it->second.get();
    }

    auto ts         = std::make_unique<TopicState>();
    ts->topicName   = topicName;
    ts->type        = type;
    TopicState* raw = ts.get();
    ctx->topicStates.emplace(topicName, std::move(ts));

    return raw;
}

/**
 * @brief Look up an existing TopicState without creating one.
 *
 * @param ctx        Session context.
 * @param topicName  DDS topic name.
 * @param type       Data type the caller intends to use the topic as.
 * @param context    Short description of the call site used in error messages.
 * @param[out] err  HSIL_OK on success, HSIL_ERR_UNKNOWN_TOPIC if the topic does
 *                  not exist, or HSIL_ERR_TOPIC_TYPE_MISMATCH on a type clash.
 * @return The TopicState, or nullptr on error.
 */
static TopicState* findTopicState(HsilContext_s* ctx,
                                  const char*    topicName,
                                  HsilTopicType  type,
                                  const char*    context,
                                  int&           err)
{
    err = HSIL_OK;

    auto it = ctx->topicStates.find(topicName);
    if (it == ctx->topicStates.end())
    {
        std::cerr << "[HSIL] " << context << ": Topic '" << topicName << "' is not known.\n";
        err = HSIL_ERR_UNKNOWN_TOPIC;

        return nullptr;
    }
    if (it->second->type != type)
    {
        std::cerr << "[HSIL] " << context << ": Topic '" << topicName << "' is already in use as a "
                  << (it->second->type == HSIL_TOPIC_GROUPED ? "grouped" : "streaming")
                  << " topic; cannot reuse it as "
                  << (type == HSIL_TOPIC_GROUPED ? "grouped" : "streaming") << " topic.\n";
        err = HSIL_ERR_TOPIC_TYPE_MISMATCH;

        return nullptr;
    }

    return it->second.get();
}

/**
 * @brief Create a DDS reader for the topic with @p qos, if one does not already exist.
 *
 * When the reader already exists @p qos is ignored (QoS is fixed at creation).
 *
 * @return true on success, false if reader creation failed.
 */
static bool createReader(HsilContext_s* ctx, TopicState* ts, const HsilQos& qos)
{
    if (ts->subscriber != nullptr)
    {
        return true;
    }

    auto sub    = std::make_unique<HsilSubscriber_s>();
    sub->ops    = ctx->ops;
    sub->ddsCtx = ctx->ddsCtx;
    sub->type   = ts->type;

    if (ts->type == HSIL_TOPIC_GROUPED)
    {
        sub->readerHandle = ctx->ops->createGroupedReader(
            ctx->ddsCtx, ts->topicName.c_str(), &qos, groupedDispatchCb, sub.get());
    }
    else
    {
        sub->readerHandle = ctx->ops->createStreamingReader(
            ctx->ddsCtx, ts->topicName.c_str(), &qos, streamingDispatchCb, sub.get());
    }

    if (sub->readerHandle == nullptr)
    {
        std::cerr << "[HSIL] Failed to create reader for topic '" << ts->topicName << "'\n";
        return false;
    }

    ts->subscriber = std::move(sub);
    
    return true;
}

/**
 * @brief Create a DDS writer for the topic with @p qos, if one does not already exist.
 *
 * When the writer already exists @p qos is ignored (QoS is fixed at creation).
 *
 * @return true on success, false if writer creation failed.
 */
static bool createWriter(HsilContext_s* ctx, TopicState* ts, const HsilQos& qos)
{
    if (ts->publisher != nullptr)
    {
        return true;
    }

    auto pub    = std::make_unique<HsilPublisher_s>();
    pub->ops    = ctx->ops;
    pub->ddsCtx = ctx->ddsCtx;
    pub->type   = ts->type;

    pub->writerHandle = (ts->type == HSIL_TOPIC_GROUPED)
        ? ctx->ops->createGroupedWriter(ctx->ddsCtx, ts->topicName.c_str(), &qos)
        : ctx->ops->createStreamingWriter(ctx->ddsCtx, ts->topicName.c_str(), &qos);

    if (pub->writerHandle == nullptr)
    {
        std::cerr << "[HSIL] Failed to create writer for topic '" << ts->topicName << "'\n";
        return false;
    }

    ts->publisher = std::move(pub);
    return true;
}

/**
 * @brief Append a GroupedData callback entry to a subscriber (thread-safe).
 */
static void addGroupedCallback(HsilSubscriber_s*       sub,
                               uint32_t                groupId,
                               HsilGroupedDataCallback callback,
                               void*                   userData)
{
    GroupedCallbackEntry entry;
    entry.groupId  = groupId;
    entry.callback = callback;
    entry.userData = userData;

    std::lock_guard<std::mutex> lock(sub->entriesMutex);
    sub->groupedEntries.push_back(entry);
}

/**
 * @brief Append a StreamingData callback entry to a subscriber (thread-safe).
 */
static void addStreamingCallback(HsilSubscriber_s*         sub,
                                 const char*               protocol,
                                 HsilStreamingDataCallback callback,
                                 void*                     userData)
{
    StreamingCallbackEntry entry;
    entry.protocol = normalizeProtocol(protocol);
    entry.callback = callback;
    entry.userData = userData;

    std::lock_guard<std::mutex> lock(sub->entriesMutex);
    sub->streamingEntries.push_back(entry);
}

/**
 * @brief Set up all DDS entities described by a parsed configuration.
 *
 * For each configured topic this determines its data type, creates a writer if
 * the agent publishes on the topic, and a reader if the agent subscribes to it.
 * No user callbacks are registered here; the application attaches them later
 * with hsil_subscribe_grouped() / hsil_subscribe_streaming().
 *
 * @return true on success, false if any DDS entity creation failed.
 */
static bool applyConfig(HsilContext_s* ctx, const HSIL::HsilSimulationConfig& cfg)
{
    for (const auto& topic : cfg.topics)
    {
        const bool hasGrouped   = !topic.publishedGrouped.empty()   || !topic.subscribedGrouped.empty();
        const bool hasStreaming = !topic.publishedStreaming.empty() || !topic.subscribedStreaming.empty();

        if (!hasGrouped && !hasStreaming)
        {
            continue; /* Topic declares no grouped or streaming endpoints. */
        }

        /* The schema guarantees a topic is not both grouped and streaming. */
        const HsilTopicType type = hasGrouped ? HSIL_TOPIC_GROUPED : HSIL_TOPIC_STREAMING;

        int err = HSIL_OK;
        TopicState* ts = getOrCreateTopicState(ctx, topic.name.c_str(), type, err);
        if (ts == nullptr)
        {
            return false;
        }

        const HsilQos topicQos = qosFromConfig(topic.qos);

        const bool publishes  = !topic.publishedGrouped.empty()  || !topic.publishedStreaming.empty();
        const bool subscribes = !topic.subscribedGrouped.empty() || !topic.subscribedStreaming.empty();

        if (publishes && !createWriter(ctx, ts, topicQos))
        {
            return false;
        }

        if (subscribes && !createReader(ctx, ts, topicQos))
        {
            return false;
        }
    }

    return true;
}

/*----------------------------------------------------------------------------*/
/* LIFECYCLE                                                                   */
/*----------------------------------------------------------------------------*/

int hsil_create(int domainId, const char* ddsConfigPath, HsilHandle* handle)
{
    try
    {
        if (handle == nullptr)
        {
            return HSIL_ERR_INVALID_ARG;
        }

        const HsilDdsOps* ops = hsil_getDdsOps();

        void* ddsCtx = ops->init(domainId, ddsConfigPath);
        if (ddsCtx == nullptr)
        {
            std::cerr << "[HSIL] DDS participant initialization failed (domain " << domainId << ")\n";
            return HSIL_ERR_DDS;
        }

        auto* ctx       = new (std::nothrow) HsilContext_s{};
        if (ctx == nullptr)
        {
            ops->destroy(ddsCtx);
            return HSIL_ERR_OUT_OF_MEMORY;
        }

        ctx->ops        = ops;
        ctx->ddsCtx     = ddsCtx;
        ctx->configMode = false;

        *handle = ctx;
        return HSIL_OK;
    }
    catch (const std::exception& e)
    {
        std::cerr << "[HSIL] hsil_create: " << e.what() << "\n";
        return HSIL_ERR_GENERIC;
    }
    catch (...)
    {
        std::cerr << "[HSIL] hsil_create: unknown exception.\n";
        return HSIL_ERR_GENERIC;
    }
}


int hsil_create_from_config(const char* configPath, const char* ddsConfigPath, HsilHandle* handle)
{
    try
    {
        if (configPath == nullptr || handle == nullptr)
        {
            return HSIL_ERR_INVALID_ARG;
        }

        HSIL::HsilSimulationConfig cfg;
        if (!HSIL::parseSimulationConfig(configPath, cfg))
        {
            return HSIL_ERR_CONFIG;
        }

        const HsilDdsOps* ops = hsil_getDdsOps();

        void* ddsCtx = ops->init(cfg.domain, ddsConfigPath);
        if (ddsCtx == nullptr)
        {
            std::cerr << "[HSIL] DDS participant initialization failed (domain " << cfg.domain << ")\n";
            return HSIL_ERR_DDS;
        }

        auto* ctx = new (std::nothrow) HsilContext_s{};
        if (ctx == nullptr)
        {
            ops->destroy(ddsCtx);
            return HSIL_ERR_OUT_OF_MEMORY;
        }

        ctx->ops        = ops;
        ctx->ddsCtx     = ddsCtx;
        ctx->configMode = true;

        if (!applyConfig(ctx, cfg))
        {
            hsil_destroy(ctx);
            return HSIL_ERR_DDS;
        }

        /* Store the parsed configuration and build O(1) lookup indices. */
        ctx->simulationConfig = std::move(cfg);
        buildSignalIndex(*ctx);

        *handle = ctx;
        return HSIL_OK;
    }
    catch (const std::exception& e)
    {
        std::cerr << "[HSIL] hsil_create_from_config: " << e.what() << "\n";
        return HSIL_ERR_GENERIC;
    }
    catch (...)
    {
        std::cerr << "[HSIL] hsil_create_from_config: unknown exception.\n";
        return HSIL_ERR_GENERIC;
    }
}


void hsil_destroy(HsilHandle handle)
{
    try
    {
        if (handle == nullptr)
        {
            return;
        }

        auto* ctx = static_cast<HsilContext_s*>(handle);

        /* Destroy all readers first, then all writers, then the participant. */
        for (auto& kv : ctx->topicStates)
        {
            if (kv.second->subscriber && kv.second->subscriber->readerHandle != nullptr)
            {
                ctx->ops->destroyReader(ctx->ddsCtx, kv.second->subscriber->readerHandle);
            }

            if (kv.second->publisher && kv.second->publisher->writerHandle != nullptr)
            {
                ctx->ops->destroyWriter(ctx->ddsCtx, kv.second->publisher->writerHandle);
            }
        }

        ctx->topicStates.clear();

        ctx->ops->destroy(ctx->ddsCtx);
        delete ctx;
    }
    catch (...)
    {
        std::cerr << "[HSIL] hsil_destroy: unknown exception.\n";
    }
}


/*----------------------------------------------------------------------------*/
/* SUBSCRIBE                                                                   */
/*----------------------------------------------------------------------------*/

int hsil_subscribe_grouped(HsilHandle              handle,
                           const char*             topicName,
                           const HsilQos*          qos,
                           uint32_t                groupId,
                           HsilGroupedDataCallback callback,
                           void*                   userData)
{
    try
    {
        if (handle == nullptr || topicName == nullptr || callback == nullptr)
        {
            return HSIL_ERR_INVALID_ARG;
        }

        auto* ctx = static_cast<HsilContext_s*>(handle);

        int err = HSIL_OK;

        /* Create new or use existing topic state. */
        TopicState* ts = getOrCreateTopicState(ctx, topicName, HSIL_TOPIC_GROUPED, err);
        if (ts == nullptr)
        {
            return err;
        }

        /* Create new or use existing subscriber. */
        if (ts->subscriber == nullptr)
        {
            const HsilQos effectiveQos = validateOrDefaultQos(qos, "hsil_subscribe_grouped");
            if (!createReader(ctx, ts, effectiveQos))
            {
                return HSIL_ERR_DDS;
            }
        }

        /* Attach the callback to the subscriber. */
        addGroupedCallback(ts->subscriber.get(), groupId, callback, userData);

        return HSIL_OK;
    }
    catch (const std::exception& e)
    {
        std::cerr << "[HSIL] hsil_subscribe_grouped: " << e.what() << "\n";
        return HSIL_ERR_GENERIC;
    }
    catch (...)
    {
        std::cerr << "[HSIL] hsil_subscribe_grouped: unknown exception.\n";
        return HSIL_ERR_GENERIC;
    }
}


int hsil_subscribe_streaming(HsilHandle                handle,
                             const char*               topicName,
                             const HsilQos*            qos,
                             const char*               protocol,
                             HsilStreamingDataCallback callback,
                             void*                     userData)
{
    try
    {
        if (handle == nullptr || topicName == nullptr || callback == nullptr)
        {
            return HSIL_ERR_INVALID_ARG;
        }

        /* Verify protocol. 
        - protocol can be null which means subscribe for any protocol.
        - if provided, it should be one of ["generic", "CAN_v1", "Eth_v1", "EthJ_v1"] */
        if ((protocol != nullptr) && !isValidProtocol(protocol))
        {
            return HSIL_ERR_INVALID_ARG;
        }

        auto* ctx = static_cast<HsilContext_s*>(handle);

        int err = HSIL_OK;

        /* Create new or use existing topic state. */
        TopicState* ts = getOrCreateTopicState(ctx, topicName, HSIL_TOPIC_STREAMING, err);
        if (ts == nullptr)
        {
            return err;
        }

        /* Create new or use existing subscriber. */
        if (ts->subscriber == nullptr)
        {
            const HsilQos effectiveQos = validateOrDefaultQos(qos, "hsil_subscribe_streaming");
            if (!createReader(ctx, ts, effectiveQos))
            {
                return HSIL_ERR_DDS;
            }
        }

        /* Attach the callback to the subscriber. */
        addStreamingCallback(ts->subscriber.get(), protocol, callback, userData);

        return HSIL_OK;
    }
    catch (const std::exception& e)
    {
        std::cerr << "[HSIL] hsil_subscribe_streaming: " << e.what() << "\n";
        return HSIL_ERR_GENERIC;
    }
    catch (...)
    {
        std::cerr << "[HSIL] hsil_subscribe_streaming: unknown exception.\n";
        return HSIL_ERR_GENERIC;
    }
}

int hsil_get_subscriber_status(HsilHandle handle, const char* topicName, HsilEndpointStatus* status)
{
    try
    {
        if (handle == nullptr || topicName == nullptr || status == nullptr)
        {
            return HSIL_ERR_INVALID_ARG;
        }

        auto* ctx = static_cast<HsilContext_s*>(handle);

        /* The status applies to grouped and streaming topics alike, so the
           topic type is not checked here. */
        auto it = ctx->topicStates.find(topicName);
        if (it == ctx->topicStates.end())
        {
            std::cerr << "[HSIL] hsil_get_subscriber_status: Topic '" << topicName << "' is not known.\n";
            return HSIL_ERR_UNKNOWN_TOPIC;
        }

        TopicState* ts = it->second.get();

        if (ts->subscriber == nullptr)
        {
            std::cerr << "[HSIL] hsil_get_subscriber_status: No subscriber found for topic '"
                      << topicName << "'. Create one with hsil_subscribe_grouped() or "
                      << "hsil_subscribe_streaming(), or declare it as subscribed in the config.\n";

            return HSIL_ERR_SUB_NOT_CREATED;
        }

        HsilSubscriber_s* sub = ts->subscriber.get();

        const int rc = sub->ops->getEndpointStatus(sub->ddsCtx, sub->readerHandle, 0, status);

        return (rc == 0) ? HSIL_OK : HSIL_ERR_DDS;
    }
    catch (const std::exception& e)
    {
        std::cerr << "[HSIL] hsil_get_subscriber_status: " << e.what() << "\n";
        return HSIL_ERR_GENERIC;
    }
    catch (...)
    {
        std::cerr << "[HSIL] hsil_get_subscriber_status: unknown exception.\n";
        return HSIL_ERR_GENERIC;
    }
}

/*----------------------------------------------------------------------------*/
/* PUBLISH                                                                     */
/*----------------------------------------------------------------------------*/

int hsil_create_grouped_publisher(HsilHandle     handle,
                                  const char*    topicName,
                                  const HsilQos* qos)
{
    try
    {
        if (handle == nullptr || topicName == nullptr)
        {
            return HSIL_ERR_INVALID_ARG;
        }

        auto* ctx = static_cast<HsilContext_s*>(handle);

        int err = HSIL_OK;
        TopicState* ts = getOrCreateTopicState(ctx, topicName, HSIL_TOPIC_GROUPED, err);
        if (ts == nullptr)
        {
            return err;
        }

        if (ts->publisher == nullptr)
        {
            const HsilQos effectiveQos = validateOrDefaultQos(qos, "hsil_create_grouped_publisher");
            if (!createWriter(ctx, ts, effectiveQos))
            {
                return HSIL_ERR_DDS;
            }
        }

        return HSIL_OK;
    }
    catch (const std::exception& e)
    {
        std::cerr << "[HSIL] hsil_create_grouped_publisher: " << e.what() << "\n";
        return HSIL_ERR_GENERIC;
    }
    catch (...)
    {
        std::cerr << "[HSIL] hsil_create_grouped_publisher: unknown exception.\n";
        return HSIL_ERR_GENERIC;
    }
}


int hsil_create_streaming_publisher(HsilHandle     handle,
                                    const char*    topicName,
                                    const HsilQos* qos)
{
    try
    {
        if (handle == nullptr || topicName == nullptr)
        {
            return HSIL_ERR_INVALID_ARG;
        }

        auto* ctx = static_cast<HsilContext_s*>(handle);

        int err = HSIL_OK;
        TopicState* ts = getOrCreateTopicState(ctx, topicName, HSIL_TOPIC_STREAMING, err);
        if (ts == nullptr)
        {
            return err;
        }

        if (ts->publisher == nullptr)
        {
            const HsilQos effectiveQos = validateOrDefaultQos(qos, "hsil_create_streaming_publisher");
            if (!createWriter(ctx, ts, effectiveQos))
            {
                return HSIL_ERR_DDS;
            }
        }

        return HSIL_OK;
    }
    catch (const std::exception& e)
    {
        std::cerr << "[HSIL] hsil_create_streaming_publisher: " << e.what() << "\n";
        return HSIL_ERR_GENERIC;
    }
    catch (...)
    {
        std::cerr << "[HSIL] hsil_create_streaming_publisher: unknown exception.\n";
        return HSIL_ERR_GENERIC;
    }
}



int hsil_get_publisher_status(HsilHandle handle, const char* topicName, HsilEndpointStatus* status)
{
    try
    {
        if (handle == nullptr || topicName == nullptr || status == nullptr)
        {
            return HSIL_ERR_INVALID_ARG;
        }

        auto* ctx = static_cast<HsilContext_s*>(handle);

        /* The status applies to grouped and streaming topics alike, so the
           topic type is not checked here. */
        auto it = ctx->topicStates.find(topicName);
        if (it == ctx->topicStates.end())
        {
            std::cerr << "[HSIL] hsil_get_publisher_status: Topic '" << topicName << "' is not known.\n";
            return HSIL_ERR_UNKNOWN_TOPIC;
        }

        TopicState* ts = it->second.get();

        if (ts->publisher == nullptr)
        {
            std::cerr << "[HSIL] hsil_get_publisher_status: No publisher found for topic '"
                      << topicName << "'. Create one with hsil_create_grouped_publisher() or "
                      << "hsil_create_streaming_publisher(), or declare it as published in the config.\n";

            return HSIL_ERR_PUB_NOT_CREATED;
        }

        HsilPublisher_s* pub = ts->publisher.get();

        const int rc = pub->ops->getEndpointStatus(pub->ddsCtx, pub->writerHandle, 1, status);

        return (rc == 0) ? HSIL_OK : HSIL_ERR_DDS;
    }
    catch (const std::exception& e)
    {
        std::cerr << "[HSIL] hsil_get_publisher_status: " << e.what() << "\n";
        return HSIL_ERR_GENERIC;
    }
    catch (...)
    {
        std::cerr << "[HSIL] hsil_get_publisher_status: unknown exception.\n";
        return HSIL_ERR_GENERIC;
    }
}

int hsil_publish_grouped(HsilHandle handle, const char* topicName, const HsilGroupedData* data)
{
    try
    {
        if (handle == nullptr || topicName == nullptr || data == nullptr)
        {
            return HSIL_ERR_INVALID_ARG;
        }

        auto* ctx = static_cast<HsilContext_s*>(handle);
        int err = HSIL_OK;

        /* Find the topic and publisher for this topic. */
        TopicState* ts = findTopicState(ctx, topicName, HSIL_TOPIC_GROUPED, "hsil_publish_grouped", err);
        if (err != HSIL_OK)
        {
            if (err == HSIL_ERR_UNKNOWN_TOPIC)
            {
                std::cerr << "[HSIL] No grouped publisher found for topic '" << topicName << "'. "
                        << "Create one with hsil_create_grouped_publisher() or declare it as "
                        << "published in the config.\n";
                err = HSIL_ERR_PUB_NOT_CREATED;
            }

            return err;
        }
        
        if ((ts == nullptr) || (ts->publisher == nullptr))
        {
            std::cerr << "[HSIL] No grouped publisher found for topic '" << topicName << "'. "
                        << "Create one with hsil_create_grouped_publisher() or declare it as "
                        << "published in the config.\n";
            
            return HSIL_ERR_PUB_NOT_CREATED;
        }

        HsilPublisher_s* pub = ts->publisher.get();

        /* Publish the grouped data. */
        const int rc = pub->ops->writeGrouped(pub->ddsCtx, pub->writerHandle, data);

        return (rc == 0) ? HSIL_OK : HSIL_ERR_DDS;
    }
    catch (const std::exception& e)
    {
        std::cerr << "[HSIL] hsil_publish_grouped: " << e.what() << "\n";
        return HSIL_ERR_GENERIC;
    }
    catch (...)
    {
        std::cerr << "[HSIL] hsil_publish_grouped: unknown exception.\n";
        return HSIL_ERR_GENERIC;
    }
}


int hsil_publish_streaming(HsilHandle handle, const char* topicName, const HsilStreamingData* data)
{
    try
    {
        if (handle == nullptr || topicName == nullptr || data == nullptr)
        {
            return HSIL_ERR_INVALID_ARG;
        }

        /* Verify protocol: should be one of ["generic", "CAN_v1", "Eth_v1", "EthJ_v1"] */
        if (!isValidProtocol(data->protocol))
        {
            return HSIL_ERR_INVALID_ARG;
        }

        auto* ctx = static_cast<HsilContext_s*>(handle);
        int err = HSIL_OK;

        /* Find the topic and publisher for this topic. */
        TopicState* ts = findTopicState(ctx, topicName, HSIL_TOPIC_STREAMING, "hsil_publish_streaming", err);
        if (err != HSIL_OK)
        {
            if (err == HSIL_ERR_UNKNOWN_TOPIC)
            {
                std::cerr << "[HSIL] No streaming publisher found for topic '" << topicName << "'. "
                        << "Create one with hsil_create_streaming_publisher() or declare it as "
                        << "published in the config.\n";
                err = HSIL_ERR_PUB_NOT_CREATED;
            }

            return err;
        }
        
        if ((ts == nullptr) || (ts->publisher == nullptr))
        {
            std::cerr << "[HSIL] No streaming publisher found for topic '" << topicName << "'. "
                    << "Create one with hsil_create_streaming_publisher() or declare it as "
                    << "published in the config.\n";
            
            return HSIL_ERR_PUB_NOT_CREATED;
        }

        HsilPublisher_s* pub = ts->publisher.get();

        /* Publish the streaming data. */
        const int rc = pub->ops->writeStreaming(pub->ddsCtx, pub->writerHandle, data);

        return (rc == 0) ? HSIL_OK : HSIL_ERR_DDS;
    }
    catch (const std::exception& e)
    {
        std::cerr << "[HSIL] hsil_publish_streaming: " << e.what() << "\n";
        return HSIL_ERR_GENERIC;
    }
    catch (...)
    {
        std::cerr << "[HSIL] hsil_publish_streaming: unknown exception.\n";
        return HSIL_ERR_GENERIC;
    }
}

/*============================================================================*/
/* CONFIG MODE – SIGNAL ACCESSOR                                               */
/*============================================================================*/

/**
 * @brief Compose a hashmap key from topic name, group id and signal name.
 *
 * Uses ASCII unit separator (0x1F) as a delimiter because it cannot appear
 * in valid JSON string values used for topic and signal names.
 */
static std::string makeSignalKey(const std::string& topicName,
                                  uint32_t           groupId,
                                  const std::string& signalName)
{
    return topicName + '\x1f' + std::to_string(groupId) + '\x1f' + signalName;
}

/**
 * @brief Build topicIndex and signalIndex from the stored simulationConfig.
 *
 * Called once after simulationConfig has been moved into the context.
 * Subscribed signals are inserted first; published signals for the same key
 * are skipped (matching the original lookup priority in hsil_read_signal_by_name()).
 */
static void buildSignalIndex(HsilContext_s& ctx)
{
    for (const auto& topic : ctx.simulationConfig.topics)
    {
        ctx.topicIndex.emplace(topic.name, &topic);

        /* Subscribed signals take priority – insert them first. */
        for (const auto& g : topic.subscribedGrouped)
        {
            for (const auto& s : g.signals)
            {
                ctx.signalIndex.emplace(makeSignalKey(topic.name, g.id, s.accessPoint), &s);
            }

            /* Cache payload size: MAX(offset + count*typeSize) over all signals in the group. */
            size_t groupSize = 0;
            for (const auto& s : g.signals)
            {
                size_t end = static_cast<size_t>(s.offset) + static_cast<size_t>(s.count) * signalTypeSize(s.type);
                if (end > groupSize) { groupSize = end; }
            }
            ctx.groupSizeCache.emplace(g.id, groupSize);
        }

        /* Published signals fill in any gaps not already covered by subscribed ones. */
        for (const auto& g : topic.publishedGrouped)
        {
            for (const auto& s : g.signals)
            {
                /* emplace is a no-op when the key already exists. */
                ctx.signalIndex.emplace(makeSignalKey(topic.name, g.id, s.accessPoint), &s);
            }

            /* Cache payload size; emplace is a no-op if subscribed version already inserted. */
            size_t groupSize = 0;
            for (const auto& s : g.signals)
            {
                size_t end = static_cast<size_t>(s.offset) + static_cast<size_t>(s.count) * signalTypeSize(s.type);
                if (end > groupSize) { groupSize = end; }
            }
            ctx.groupSizeCache.emplace(g.id, groupSize);
        }
    }
}

/**
 * @brief Return the byte size of a single element of the given public signal type.
 *
 * Returns 0 for unknown types so the caller can detect unsupported types.
 */
static size_t signalTypeSize(HsilSignalType type)
{
    switch (type)
    {
        case HSIL_SIGNAL_BOOL:    return 1;
        case HSIL_SIGNAL_INT8:    return 1;
        case HSIL_SIGNAL_INT16:   return 2;
        case HSIL_SIGNAL_INT32:   return 4;
        case HSIL_SIGNAL_INT64:   return 8;
        case HSIL_SIGNAL_UINT8:   return 1;
        case HSIL_SIGNAL_UINT16:  return 2;
        case HSIL_SIGNAL_UINT32:  return 4;
        case HSIL_SIGNAL_UINT64:  return 8;
        case HSIL_SIGNAL_FLOAT32: return 4;
        case HSIL_SIGNAL_FLOAT64: return 8;
        case HSIL_SIGNAL_BYTE:    return 1;
        case HSIL_SIGNAL_CHAR:    return 1;
        default:                  return 0;
    }
}


HsilSignalConversion hsil_check_signal_conversion(HsilSignalType fromType, HsilSignalType toType)
{
    try
    {
        static const HsilSignalConversion OK = HSIL_SIGNAL_CONV_OK;
        static const HsilSignalConversion LO = HSIL_SIGNAL_CONV_LOSSY;
        static const HsilSignalConversion NO = HSIL_SIGNAL_CONV_INVALID;

        /*  Conversion result: table[fromType][toType]
        *
        *  OK = exact / lossless (✔ or 0/1 in spec)
        *  LO = lossy, warn      (⚠️ in spec)
        *  NO = invalid, error   (❌ in spec)
        *
        *  Source: HSIL-CoSim.md § "Handling Datatype Mismatch"
        *
        *              to→  BOOL  INT8    INT16   INT32   INT64   UINT8   UINT16  UINT32  UINT64  FLOAT32 FLOAT64 BYTE    CHAR   */
        static const HsilSignalConversion table[13][13] = {
        /* BOOL   from↓ */ { OK,   OK,     OK,     OK,     OK,     OK,     OK,     OK,     OK,     NO,     NO,     NO,     NO  },
        /* INT8         */ { OK,   OK,     OK,     OK,     OK,     LO,     LO,     LO,     LO,     OK,     OK,     OK,     NO  },
        /* INT16        */ { OK,   LO,     OK,     OK,     OK,     LO,     LO,     LO,     LO,     OK,     OK,     NO,     NO  },
        /* INT32        */ { OK,   LO,     LO,     OK,     OK,     LO,     LO,     LO,     LO,     LO,     OK,     NO,     NO  },
        /* INT64        */ { OK,   LO,     LO,     LO,     OK,     LO,     LO,     LO,     LO,     LO,     LO,     NO,     NO  },
        /* UINT8        */ { OK,   LO,     OK,     OK,     OK,     OK,     OK,     OK,     OK,     OK,     OK,     OK,     NO  },
        /* UINT16       */ { OK,   LO,     LO,     OK,     OK,     LO,     OK,     OK,     OK,     OK,     OK,     NO,     NO  },
        /* UINT32       */ { OK,   LO,     LO,     LO,     OK,     LO,     LO,     OK,     OK,     LO,     OK,     NO,     NO  },
        /* UINT64       */ { OK,   LO,     LO,     LO,     LO,     LO,     LO,     LO,     OK,     LO,     LO,     NO,     NO  },
        /* FLOAT32      */ { NO,   LO,     LO,     LO,     LO,     LO,     LO,     LO,     LO,     OK,     OK,     NO,     NO  },
        /* FLOAT64      */ { NO,   LO,     LO,     LO,     LO,     LO,     LO,     LO,     LO,     LO,     OK,     NO,     NO  },
        /* BYTE         */ { NO,   NO,     NO,     NO,     NO,     NO,     NO,     NO,     NO,     NO,     NO,     OK,     LO  },
        /* CHAR         */ { NO,   NO,     NO,     NO,     NO,     NO,     NO,     NO,     NO,     NO,     NO,     OK,     OK  },
        };

        HsilSignalConversion Ret = HSIL_SIGNAL_CONV_INVALID;

        if (fromType >= 0 && toType >= 0 && fromType < 13 && toType < 13)
        {
            Ret = table[fromType][toType];
        }

        /* Log warnings and errors for lossy and invalid conversions, respectively. */
        switch(Ret)
        {
            case HSIL_SIGNAL_CONV_OK:
                break; // No log for exact conversions.

            case HSIL_SIGNAL_CONV_LOSSY:
                std::cerr << "[HSIL] Warning: lossy conversion from " << fromType << " to " << toType << ".\n";
                break;

            case HSIL_SIGNAL_CONV_INVALID:
            default:
                std::cerr << "[HSIL] Error: invalid conversion from " << fromType << " to " << toType << ".\n";
                break;
        }   

        return Ret;
    }
    catch (const std::exception& e)
    {
        std::cerr << "[HSIL] hsil_check_signal_conversion: " << e.what() << "\n";
        return HSIL_SIGNAL_CONV_INVALID;
    }
    catch (...)
    {
        std::cerr << "[HSIL] hsil_check_signal_conversion: unknown exception.\n";
        return HSIL_SIGNAL_CONV_INVALID;
    }
}



int hsil_get_grouped_data_size(HsilHandle handle,
                               uint32_t   groupId,
                               size_t*    dataSize)
{
    try
    {
        if ((handle == nullptr) || (dataSize == nullptr))
        {
            return HSIL_ERR_INVALID_ARG;
        }

        HsilContext_s* ctx = static_cast<HsilContext_s*>(handle);

        if (!ctx->configMode)
        {
            std::cerr << "[HSIL] hsil_get_grouped_data_size: session was not created from a config file.\n";
            return HSIL_ERR_CONFIG;
        }

        auto it = ctx->groupSizeCache.find(groupId);
        if (it == ctx->groupSizeCache.end())
        {
            std::cerr << "[HSIL] hsil_get_grouped_data_size: group id " << groupId << " not found in config.\n";
            return HSIL_ERR_INVALID_ARG;
        }

        *dataSize = it->second;
        
        return HSIL_OK;
    }
    catch (const std::exception& e)
    {
        std::cerr << "[HSIL] hsil_get_grouped_data_size: " << e.what() << "\n";
        return HSIL_ERR_GENERIC;
    }
    catch (...)
    {
        std::cerr << "[HSIL] hsil_get_grouped_data_size: unknown exception.\n";
        return HSIL_ERR_GENERIC;
    }
}



int hsil_read_signal_by_name(HsilHandle             handle,
                             const char*            topicName,
                             const HsilGroupedData* data,
                             const char*            signalName,
                             size_t                 arrayIndex,
                             HsilSignalValue*       value,
                             HsilSignalType*        type)
{
    try
    {
        if ((handle == nullptr) || (topicName == nullptr) || (signalName == nullptr) ||
            (data == nullptr) || (data->data == nullptr) || (value == nullptr) || (type == nullptr))
        {
            return HSIL_ERR_INVALID_ARG;
        }

        HsilContext_s* ctx = static_cast<HsilContext_s*>(handle);

        /* Is a valid topic? */
        if (ctx->topicStates.find(topicName) == ctx->topicStates.end())
        {
            std::cerr << "[HSIL] hsil_read_signal_by_name: topic '" << topicName << "' not found in config.\n";
            return HSIL_ERR_UNKNOWN_TOPIC;
        }

        /* O(1) signal lookup using the pre-built index. */
        auto signalIt = ctx->signalIndex.find(makeSignalKey(topicName, data->id, signalName));
        if (signalIt == ctx->signalIndex.end())
        {
            std::cerr << "[HSIL] hsil_read_signal_by_name: signal '" << signalName << "' not found in config for topic '" << topicName << "' and group id " << data->id << ".\n";
            return HSIL_ERR_UNKNOWN_SIGNAL;
        }

        /* Is arrayIndex valid (for array signals)? */
        if (arrayIndex >= static_cast<size_t>(signalIt->second->count))
        {
            std::cerr << "[HSIL] hsil_read_signal_by_name: arrayIndex " << arrayIndex
                    << " out of range for signal '" << signalName << "' (count=" << signalIt->second->count << ").\n";
            return HSIL_ERR_GENERIC;
        }

        size_t signalSize = signalTypeSize(signalIt->second->type);
        size_t signalIndex = static_cast<size_t>(signalIt->second->offset) + (arrayIndex * signalSize);
        
        /* Is signal index valid (signal lies within the packet)? */
        if ((signalIndex + signalSize) > data->dataSize)
        {
            std::cerr << "[HSIL] hsil_read_signal_by_name: signal index " << signalIndex
                    << " with size " << signalSize << " is out of range for data buffer with size " << data->dataSize << ".\n";
            return HSIL_ERR_GENERIC;
        }
        
        /* Read the signal from the grouped data packet. */
        value->asUint64 = 0;    // 0 initialize the largetst member to ensure the complete union is zero-initialized.
        memcpy(value, (data->data + signalIndex), signalSize);
        *type = signalIt->second->type;

        return HSIL_OK;
    }
    catch (const std::exception& e)
    {
        std::cerr << "[HSIL] hsil_read_signal_by_name: " << e.what() << "\n";
        return HSIL_ERR_GENERIC;
    }
    catch (...)
    {
        std::cerr << "[HSIL] hsil_read_signal_by_name: unknown exception.\n";
        return HSIL_ERR_GENERIC;
    }
}



int hsil_write_signal_by_name(HsilHandle             handle,
                              const char*            topicName,
                              HsilGroupedData*       data,
                              const char*            signalName,
                              size_t                 arrayIndex,
                              const HsilSignalValue  value,
                              const HsilSignalType   type)
{
    try
    {
        int Ret = HSIL_OK;
        
        if ((handle == nullptr) || (topicName == nullptr) || (signalName == nullptr) ||
            (data == nullptr) || (data->data == nullptr))
        {
            return HSIL_ERR_INVALID_ARG;
        }

        HsilContext_s* ctx = static_cast<HsilContext_s*>(handle);

        /* Is a valid topic? */
        if (ctx->topicStates.find(topicName) == ctx->topicStates.end())
        {
            std::cerr << "[HSIL] hsil_write_signal_by_name: topic '" << topicName << "' not found in config.\n";
            return HSIL_ERR_UNKNOWN_TOPIC;
        }

        /* O(1) signal lookup using the pre-built index. */
        auto signalIt = ctx->signalIndex.find(makeSignalKey(topicName, data->id, signalName));
        if (signalIt == ctx->signalIndex.end())
        {
            std::cerr << "[HSIL] hsil_write_signal_by_name: signal '" << signalName << "' not found in config for topic '" << topicName << "' and group id " << data->id << ".\n";
            return HSIL_ERR_UNKNOWN_SIGNAL;
        }

        /* Is arrayIndex valid (for array signals)? */
        if (arrayIndex >= static_cast<size_t>(signalIt->second->count))
        {
            std::cerr << "[HSIL] hsil_write_signal_by_name: arrayIndex " << arrayIndex
                    << " out of range for signal '" << signalName << "' (count=" << signalIt->second->count << ").\n";
            return HSIL_ERR_GENERIC;
        }

        /* Check signal conversion */
        HsilSignalConversion ConvRes = hsil_check_signal_conversion(type, signalIt->second->type);
        switch(ConvRes)
        {
            case HSIL_SIGNAL_CONV_OK: {break;} // No log for exact conversions.
            case HSIL_SIGNAL_CONV_LOSSY: { Ret = HSIL_WARN_SIGNAL_CONVERSION; break; }  // Lossy conversion, log warning but continue with the write.
            case HSIL_SIGNAL_CONV_INVALID: { return HSIL_ERR_SIGNAL_CONVERSION; }   // Invalid conversion, log error and return.
            default:
            {
                std::cerr << "[HSIL] Error: Unknown signal conversion result " << ConvRes << ".\n";
                return HSIL_ERR_GENERIC;
            }
        }

        size_t signalSize = signalTypeSize(signalIt->second->type);
        size_t signalIndex = static_cast<size_t>(signalIt->second->offset) + (arrayIndex * signalSize);
        
        /* Is signal index valid (signal lies within the packet)? */
        if ((signalIndex + signalSize) > data->dataSize)
        {
            std::cerr << "[HSIL] hsil_write_signal_by_name: signal index " << signalIndex
                    << " with size " << signalSize << " is out of range for data buffer with size " << data->dataSize << ".\n";
            return HSIL_ERR_GENERIC;
        }

        /* Write the signal to the grouped data packet. */
        memcpy((data->data + signalIndex), &value, signalSize);

        return Ret;
    }
    catch (const std::exception& e)
    {
        std::cerr << "[HSIL] hsil_write_signal_by_name: " << e.what() << "\n";
        return HSIL_ERR_GENERIC;
    }
    catch (...)
    {
        std::cerr << "[HSIL] hsil_write_signal_by_name: unknown exception.\n";
        return HSIL_ERR_GENERIC;
    }
}
