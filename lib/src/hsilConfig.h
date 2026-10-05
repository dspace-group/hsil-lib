/** <!-------------------------------------------------------------------------->
*
*   @file hsilConfig.h
*
*   @brief Data structures and parser for the HSIL JSON simulation configuration.
*
*   @author
*       dSPACE SE & Co. KG
*
*   @description
*       Declares the C++ data structures that mirror the HSIL simulation
*       configuration JSON schema and exposes parseSimulationConfig() which
*       reads a configuration file from disk and populates those structures.
*
*   @copyright
*       Copyright 2026, dSPACE SE & Co. KG. All rights reserved.
*
*   <hr><br>
*<!-------------------------------------------------------------------------->*/

#pragma once

#include <hsil/hsil.h>

#include <cstdint>
#include <string>
#include <vector>

namespace HSIL
{

/*----------------------------------------------------------------------------*/
/* PRIMITIVE DATA TYPE ENUMERATION                                             */
/*----------------------------------------------------------------------------*/

/**
 * @brief Convert a schema type name string to the HsilSignalType enumeration.
 *
 * @param name  Lower-case type name as it appears in the JSON ("int32", etc.).
 * @return The matching HsilSignalType, or HsilSignalType::HSIL_SIGNAL_UNKNOWN if not recognized.
 */
HsilSignalType parseDataType(const std::string& name);

/*----------------------------------------------------------------------------*/
/* SIGNAL CONFIG                                                               */
/*----------------------------------------------------------------------------*/

/**
 * @brief Describes one signal entry packed inside a GroupedData payload.
 */
struct SignalConfig
{
    std::string    accessPoint; /**< Agent-internal access point (address, symbol, …). */
    int            offset;      /**< Byte offset within the GroupedData data array.    */
    HsilSignalType type;        /**< Data type of the signal element.                  */
    int            count;       /**< Number of elements; 1 for scalars, >1 for arrays. */
};

/*----------------------------------------------------------------------------*/
/* GROUPED DATA CONFIG                                                         */
/*----------------------------------------------------------------------------*/

/**
 * @brief Describes one GroupedData instance that a topic publishes or subscribes to.
 */
struct GroupedDataConfig
{
    uint32_t                 id;      /**< Globally unique data-group identifier.    */
    std::vector<SignalConfig> signals; /**< Signal entries packed into the payload.  */
};

/*----------------------------------------------------------------------------*/
/* STREAMING DATA CONFIG                                                       */
/*----------------------------------------------------------------------------*/

/**
 * @brief Describes one StreamingData instance that a topic publishes or subscribes to.
 */
struct StreamingDataConfig
{
    std::string protocol;    /**< Protocol identifier string (max. 8 chars, e.g. "CAN_v1"). */
    std::string accessPoint; /**< Agent-internal access point for the stream endpoint.       */
};

/*----------------------------------------------------------------------------*/
/* QOS CONFIG                                                                  */
/*----------------------------------------------------------------------------*/

/**
 * @brief Quality-of-Service hints for a DDS topic.
 *
 * Fields that are left empty or zero will be interpreted as "use DDS default".
 */
struct QosConfig
{
    std::string priority;    /**< "high" or "low" (advisory).                       */
    std::string reliability; /**< "reliable" or "fast" (best-effort).               */
    int         queueLength; /**< History queue depth; 0 means use DDS default.     */
};

/*----------------------------------------------------------------------------*/
/* TOPIC CONFIG                                                                */
/*----------------------------------------------------------------------------*/

/**
 * @brief Full description of one DDS topic derived from the configuration file.
 *
 * A topic carries either GroupedData OR StreamingData instances – never a mix
 * of both types. The parser enforces this by populating either the grouped or
 * the streaming member vectors, never both.
 */
struct TopicConfig
{
    std::string name;      /**< DDS topic name.                                              */
    std::string customKey; /**< Optional key linking this topic to applicationSpecifics.     */
    QosConfig   qos;       /**< QoS overrides for this topic.                               */

    /** GroupedData instances this agent publishes on this topic. */
    std::vector<GroupedDataConfig>   publishedGrouped;

    /** GroupedData instances this agent subscribes to on this topic. */
    std::vector<GroupedDataConfig>   subscribedGrouped;

    /** StreamingData instances this agent publishes on this topic. */
    std::vector<StreamingDataConfig> publishedStreaming;

    /** StreamingData instances this agent subscribes to on this topic. */
    std::vector<StreamingDataConfig> subscribedStreaming;
};

/*----------------------------------------------------------------------------*/
/* TOP-LEVEL SIMULATION CONFIG                                                 */
/*----------------------------------------------------------------------------*/

/**
 * @brief Fully parsed representation of an HSIL JSON simulation configuration.
 */
struct HsilSimulationConfig
{
    int                     version; /**< Value of $hsil-simulation-configuration-version. */
    int                     domain;  /**< DDS domain identifier.                           */
    std::vector<TopicConfig> topics;  /**< All configured topics.                          */
};

/*----------------------------------------------------------------------------*/
/* PARSER FUNCTION                                                             */
/*----------------------------------------------------------------------------*/

/**
 * @brief Parse an HSIL JSON simulation configuration file into structured data.
 *
 * Opens @p path, parses the JSON, and populates @p config. Parsing errors are
 * written to stderr. The function does not throw.
 *
 * @param path    Path to the JSON configuration file.
 * @param config  Output parameter; populated on success.
 * @return true on success, false if the file cannot be opened or is malformed.
 */
bool parseSimulationConfig(const std::string& path, HsilSimulationConfig& config);

} // namespace HSIL
