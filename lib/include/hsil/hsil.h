// SPDX-FileCopyrightText: 2026 dSPACE SE & Co. KG
// SPDX-License-Identifier: Apache-2.0

/** <!-------------------------------------------------------------------------->
*
*   @file hsil.h
*
*   @brief Public C API for the HSIL CoSim library.
*
*   @author
*       dSPACE SE & Co. KG
*
*   @description
*       Single-include header that gives simulation agents easy access to the
*       HSIL Co-Simulation DDS network. Supports both a without hsil configuration 
*       (explicit subscribe / publish calls) and with hsil configuration (driven by 
*       a JSON simulation configuration file, as specified by the schema file in
*       the HSIL Specification).
*
*       Without hsil configuration example
*       ----------------------------------
*       @code
*       HsilHandle h;
*       hsil_create(42, NULL, &h);
*
*       // Subscribe to GroupedData (HSIL_ALL_IDS = accept every group id)
*       hsil_subscribe_grouped(h, "SensorTopic", NULL, HSIL_ALL_IDS, onSensorData, NULL);
*
*       // Create a publisher for this topic
*       hsil_create_streaming_publisher(h, "CAN_Topic", NULL);
*
*       // Publish StreamingData at run-time
*       HsilStreamingData frame = { .protocol = "CAN_v1", ... };
*       hsil_publish_streaming(h, "CAN_Topic", &frame);
*
*       hsil_destroy(h);  // publishers and subscribers are freed automatically
*       @endcode
*
*       With hsil configuration example
*       -------------------------------
*       @code
*       HsilHandle h;
*       hsil_create_from_config("simulation.json", NULL, &h);
*
*       // Entities already exist – just attach callbacks and publish.
*       hsil_subscribe_grouped(h, "SensorTopic", NULL, HSIL_ALL_IDS, onSensorData, NULL);
*       hsil_publish_streaming(h, "CAN_Topic", &frame);
*
*       hsil_destroy(h);
*       @endcode
*
*   <hr><br>
*<!-------------------------------------------------------------------------->*/

#ifndef HSIL_COSIM_H
#define HSIL_COSIM_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

/*----------------------------------------------------------------------------*/
/* API VISIBILITY MACRO                                                        */
/*----------------------------------------------------------------------------*/

/**
 * @brief Controls symbol visibility for shared library builds.
 *
 * When building the shared library (hsil_cosim.so / hsil_cosim.dll), the
 * CMake target defines HSIL_BUILDING_SHARED so all public symbols get the
 * correct export attribute.  Consumers of the installed shared library may
 * define HSIL_USING_SHARED before including this header to get the import
 * attribute on Windows; on ELF platforms the linker resolves shared-library
 * symbols without explicit import attributes.
 */
#if defined(_WIN32) || defined(__CYGWIN__)
#  if defined(HSIL_BUILDING_SHARED)
#    define HSIL_API __declspec(dllexport)
#  elif defined(HSIL_USING_SHARED)
#    define HSIL_API __declspec(dllimport)
#  else
#    define HSIL_API
#  endif
#elif defined(__GNUC__) && __GNUC__ >= 4
#  define HSIL_API __attribute__((visibility("default")))
#else
#  define HSIL_API
#endif

/*----------------------------------------------------------------------------*/
/* ERROR CODES                                                                 */
/*----------------------------------------------------------------------------*/

/**< Signal value was converted with possible loss of precision. */
#define HSIL_WARN_SIGNAL_CONVERSION 1  

/** Returned by all API functions on success. */
#define HSIL_OK                  0

/** A general, unspecified error occurred. */
#define HSIL_ERR_GENERIC        -1

/** A supplied argument was NULL or otherwise invalid. */
#define HSIL_ERR_INVALID_ARG    -2

/** The configuration file could not be opened, read, or parsed. */
#define HSIL_ERR_CONFIG         -3

/** An underlying DDS operation failed. */
#define HSIL_ERR_DDS            -4

/** The requested topic name was not found in the configuration. */
#define HSIL_ERR_UNKNOWN_TOPIC  -5

/** Memory allocation failed. */
#define HSIL_ERR_OUT_OF_MEMORY  -6

/** The requested signal name was not found in the config for the given topic and group id. */
#define HSIL_ERR_UNKNOWN_SIGNAL -7

/** A topic was used with the wrong data type (grouped used as streaming or vice versa). */
#define HSIL_ERR_TOPIC_TYPE_MISMATCH -8

/** A publisher was not created for the requested topic. */
#define HSIL_ERR_PUB_NOT_CREATED -9

/** A signal value conversion is not possible. */
#define HSIL_ERR_SIGNAL_CONVERSION -10

/** A subscriber was not created for the requested topic. */
#define HSIL_ERR_SUB_NOT_CREATED -11

/*----------------------------------------------------------------------------*/
/* SENTINEL VALUES                                                            */
/*----------------------------------------------------------------------------*/

/**
 * @brief Wildcard value for the groupId parameter of the grouped subscriber functions.
 *
 * When passed as the groupId, the callback is invoked for every received
 * GroupedData sample, regardless of its id field.
 */
#define HSIL_ALL_IDS  (0xFFFFFFFFu)

/**
 * @brief Aggregate initialiser for the default QoS settings.
 *
 * Expands to a brace-enclosed initialiser list; use as:
 * @code
 *   HsilQos q = HSIL_DEFAULT_QOS;
 * @endcode
 */
#define HSIL_DEFAULT_QOS { HSIL_QOS_PRIORITY_NORMAL, 1, HSIL_QOS_RELIABILITY_FAST}

/*----------------------------------------------------------------------------*/
/* OPAQUE HANDLES                                                              */
/*----------------------------------------------------------------------------*/

/**
 * @brief Opaque handle representing an initialized HSIL CoSim session.
 *
 * Obtain one via hsil_create() or hsil_create_from_config(). Release it with
 * hsil_destroy() when the session is no longer needed.
 */
typedef struct HsilContext_s* HsilHandle;

/*----------------------------------------------------------------------------*/
/* DATA TYPES                                                                  */
/*----------------------------------------------------------------------------*/

/**
 * @brief Data type carried by a DDS topic.
 *
 * A topic carries either GroupedData or StreamingData – never both. The type
 * is fixed the first time when a publisher or subscriber of a topic is created. 
 * Using a topic with the wrong type returns HSIL_ERR_TOPIC_TYPE_MISMATCH.
 */
typedef enum
{
    HSIL_TOPIC_GROUPED   = 0, /**< Topic carries GroupedData samples.   */
    HSIL_TOPIC_STREAMING = 1  /**< Topic carries StreamingData samples. */
} HsilTopicType;

/**
 * @brief Can Frame type which is a part of the CAN streaming meta data.
 */
typedef enum
{
    HSIL_CAN_FRAME_STD = 0,         /**< Standard CAN frame. */
    HSIL_CAN_FRAME_EXT = 1,         /**< Extended CAN frame (29-bit ID). */
    HSIL_CAN_FRAME_FD = 2,          /**< CAN FD frame. */
    HSIL_CAN_FRAME_EXT_FD = 3,      /**< Extended CAN FD frame. */
    HSIL_CAN_FRAME_FD_BRS = 6,      /**< CAN FD frame with Bit Rate Switch. */
    HSIL_CAN_FRAME_EXT_FD_BRS = 7   /**< Extended CAN FD frame with Bit Rate Switch. */
} HsilCanFrameType;

/**
 * @brief Ethernet frame flags which are a part of the Eth_v1 and EthJ_v1 streaming meta data.
 */
typedef enum
{
    HSIL_ETH_FLAG_HAS_FCS = 0x01  /**< Set if the frame payload includes the Frame Check Sequence. */
} HsilEthFlags;

/**
 * @brief Meta data layout for the CAN streaming packet.
 *
 */
typedef struct
{
    uint32_t          messageId;  /**< CAN message ID (little-endian).                   */
    HsilCanFrameType  frameType;  /**< Frame type; one of the HSIL_CAN_FRAME_* values.   */
} HsilCanMetaData;

/**
 * @brief Meta data layout for the Ethernet streaming packet.
 *
 */
typedef struct
{
    uint8_t flags;  /**< Ethernet flags; bitwise-OR of HSIL_ETH_FLAG_* values. */
} HsilEthMetaData;

/**
 * @brief Meta data layout for the Jumbo Ethernet streaming packet.
 *
 * Wire layout:
 *   Bytes 0–3 : mtu   – maximum transmission unit of the jumbo frame, little-endian uint32.
 *   Byte  4   : flags – same bitmask as HsilEthMeta.flags.
 */
typedef struct
{
    uint32_t mtu;    /**< Jumbo frame MTU (little-endian).                       */
    uint8_t  flags;  /**< Ethernet flags; bitwise-OR of HSIL_ETH_FLAG_* values. */
} HsilEthJMetaData;

/**
 * @brief Meta data layout for the generic streaming packet.
 */
typedef struct
{
    size_t   size;       /**< Size of the metadata in bytes. */
    uint8_t* data;      /**< Pointer to a buffer containing the metadata. */
} HsilGenericMetaData;

/**
 * @brief It represents the metadata data structure for the streaming packet.
 *        Can hold the meta data for all supported streaming protocols (CAN, Ethernet, Jumbo Ethernet).
 */
typedef union
{
    HsilGenericMetaData generic;    /**< meta data for generic protocol. */
    HsilCanMetaData     can;        /**< meta data for CAN protocol. */
    HsilEthMetaData     eth;        /**< meta data for Ethernet protocol. */
    HsilEthJMetaData    ethj;       /**< meta data for Jumbo Ethernet protocol. */
} HsilStreamingMetaData;

/**
 * @brief In-memory representation of a StreamingData DDS sample.
 *
 * Mirrors the StreamingData struct defined in the HSIL CoSim IDL. All pointer
 * fields are valid only for the duration of the callback that delivers the
 * sample.
 */
typedef struct
{
    char                   protocol[8];    /**< Protocol identifier, e.g. "generic", "CAN_v1", "Eth_v1" or "EthJ_v1". */
    uint64_t               timestamp;      /**< Simulation timestamp in nanoseconds. */
    HsilStreamingMetaData  meta;           /**< Protocol-specific meta data. */
    const uint8_t*         data;           /**< Raw payload bytes. */
    size_t                 dataSize;       /**< Number of bytes in data. */
} HsilStreamingData;

/**
 * @brief In-memory representation of a GroupedData DDS sample.
 *
 * Mirrors the GroupedData struct defined in the HSIL CoSim IDL. All pointer
 * fields are valid only for the duration of the callback that delivers the
 * sample; do not store raw pointers beyond the callback scope. Copy the
 * payload if persistence is required.
 */
typedef struct
{
    uint32_t        id;             /**< Unique data-group identifier.                          */
    uint32_t        sequenceNumber; /**< Monotonically increasing application-level counter.    */
    uint64_t        timestamp;      /**< Simulation timestamp in nanoseconds.                   */
    uint8_t*        data;           /**< Packed signal payload (see configuration for layout).  */
    size_t          dataSize;       /**< Number of bytes in data.                               */
} HsilGroupedData;

/**
 * @brief Processing priority hint for a DDS topic.
 *
 * Maps to the "priority" field of the QoS specification in the simulation
 * configuration schema. The DDS vendor backend translates this to an
 * appropriate transport priority value.
 */
typedef enum
{
    HSIL_QOS_PRIORITY_NORMAL = 0, /**< Default; standard scheduling. */
    HSIL_QOS_PRIORITY_LOW    = 1, /**< Below-normal scheduling.      */
    HSIL_QOS_PRIORITY_HIGH   = 2  /**< Above-normal scheduling.      */
} HsilQosPriority;

/**
 * @brief Reliability policy for a DDS topic.
 *
 * Maps to the "reliability" field of the QoS specification.
 * Fast (best-effort) communication is simpler but may lose packets.
 * Reliable communication uses acknowledgements to ensure no values are lost.
 */
typedef enum
{
    HSIL_QOS_RELIABILITY_FAST     = 0, /**< Best-effort; no acknowledgements. Default. */
    HSIL_QOS_RELIABILITY_RELIABLE = 1  /**< Acknowledged; no sample loss.              */
} HsilQosReliability;

/**
 * @brief Quality-of-Service settings for a DDS reader or writer.
 *
 * Pass a pointer to this struct to the subscribe / publish creation functions.
 * NULL is accepted and causes the default QoS to be applied with a warning.
 * Invalid enum values are also replaced with their defaults and a warning is
 * emitted.
 *
 * Initialise from the default with:
 * @code
 *   HsilQos q = HSIL_DEFAULT_QOS;
 *   q.priority = HSIL_QOS_PRIORITY_HIGH;
 * @endcode
 */
typedef struct
{
    HsilQosPriority    priority;    /**< Processing priority hint; default is NORMAL.        */
    int                queueLength; /**< History queue depth (>= 1); default is 1.           */
    HsilQosReliability reliability; /**< Reliability policy; default is FAST (best-effort).  */
} HsilQos;

/**
 * @brief QoS policy that caused the most recent incompatibility.
 *
 * Between two HSIL agents only reliability can be incompatible: a FAST
 * publisher does not satisfy a RELIABLE subscriber. Priority and queue length
 * never clash. Any other policy reported by the DDS backend is mapped to
 * HSIL_QOS_MISMATCH_OTHER.
 */
typedef enum
{
    HSIL_QOS_MISMATCH_NONE        = 0, /**< No incompatibility recorded.               */
    HSIL_QOS_MISMATCH_RELIABILITY = 1, /**< FAST publisher versus RELIABLE subscriber. */
    HSIL_QOS_MISMATCH_OTHER       = 2  /**< Another, non-HSIL QoS policy.              */
} HsilQosMismatchPolicy;

/**
 * @brief Discovery and QoS compatibility status of a publisher or subscriber.
 *
 * Filled in by hsil_get_publisher_status() / hsil_get_subscriber_status().
 * Counts refer to remote endpoints, not to remote applications.
 */
typedef struct
{
    int                   matchedCount;            /**< Remote endpoints currently matched; live value. */
    int                   qosMismatchCount;        /**< Remote endpoints rejected for QoS; cumulative.  */
    HsilQosMismatchPolicy lastMismatchedQosPolicy; /**< Policy behind the most recent rejection.        */
} HsilEndpointStatus;


/**
 * @brief Wire type tag returned alongside an extracted signal value.
 *
 * Mirrors the primitiveDataTypeId enumeration from the HSIL JSON schema.
 * The tag indicates which member of HsilSignalValue is active.
 */
typedef enum
{
    HSIL_SIGNAL_BOOL    =  0, /**< Boolean; stored as uint8_t (0 = false, 1 = true). */
    HSIL_SIGNAL_INT8    =  1,
    HSIL_SIGNAL_INT16   =  2,
    HSIL_SIGNAL_INT32   =  3,
    HSIL_SIGNAL_INT64   =  4,
    HSIL_SIGNAL_UINT8   =  5,
    HSIL_SIGNAL_UINT16  =  6,
    HSIL_SIGNAL_UINT32  =  7,
    HSIL_SIGNAL_UINT64  =  8,
    HSIL_SIGNAL_FLOAT32 =  9,
    HSIL_SIGNAL_FLOAT64 = 10,
    HSIL_SIGNAL_BYTE    = 11,
    HSIL_SIGNAL_CHAR    = 12,
    HSIL_SIGNAL_UNKNOWN = 13 /**< Returned when the type string was not recognized. */
} HsilSignalType;

/**
 * @brief Holds a single scalar value extracted from a GroupedData payload.
 *
 * Read the member that corresponds to the HsilSignalType returned by
 * hsil_read_signal_by_name(). For array signals, call hsil_read_signal_by_name() once per
 * element, incrementing elementIndex each time.
 */
typedef union
{
    uint8_t  asBool;    /**< Active when type == HSIL_SIGNAL_BOOL.    */
    int8_t   asInt8;    /**< Active when type == HSIL_SIGNAL_INT8.    */
    int16_t  asInt16;   /**< Active when type == HSIL_SIGNAL_INT16.   */
    int32_t  asInt32;   /**< Active when type == HSIL_SIGNAL_INT32.   */
    int64_t  asInt64;   /**< Active when type == HSIL_SIGNAL_INT64.   */
    uint8_t  asUint8;   /**< Active when type == HSIL_SIGNAL_UINT8.   */
    uint16_t asUint16;  /**< Active when type == HSIL_SIGNAL_UINT16.  */
    uint32_t asUint32;  /**< Active when type == HSIL_SIGNAL_UINT32.  */
    uint64_t asUint64;  /**< Active when type == HSIL_SIGNAL_UINT64.  */
    float    asFloat32; /**< Active when type == HSIL_SIGNAL_FLOAT32. */
    double   asFloat64; /**< Active when type == HSIL_SIGNAL_FLOAT64. */
    uint8_t  asByte;    /**< Active when type == HSIL_SIGNAL_BYTE.    */
    char     asChar;    /**< Active when type == HSIL_SIGNAL_CHAR.    */
} HsilSignalValue;

typedef enum
{
    HSIL_SIGNAL_CONV_OK = 0, /**< No conversion needed; types match exactly. */
    HSIL_SIGNAL_CONV_LOSSY,   /**< Conversion performed with some loss of precision. */
    HSIL_SIGNAL_CONV_INVALID  /**< Conversion not possible; types are incompatible. */
} HsilSignalConversion;


/*----------------------------------------------------------------------------*/
/* CALLBACK TYPES                                                              */
/*----------------------------------------------------------------------------*/

/**
 * @brief Callback invoked when a GroupedData sample arrives on a subscribed topic.
 *
 * The callback is called from an internal DDS receive thread. Implementations
 * must be thread-safe and should not block for a long time. The @p data pointer
 * is only valid inside the callback body.
 *
 * @param topicName  Null-terminated DDS topic name.
 * @param data       Received sample; valid only for the lifetime of this call.
 * @param userData   Opaque pointer supplied at subscription time.
 */
typedef void (*HsilGroupedDataCallback)(const char*           topicName,
                                        const HsilGroupedData* data,
                                        void*                  userData);

/**
 * @brief Callback invoked when a StreamingData sample arrives on a subscribed topic.
 *
 * @param topicName  Null-terminated DDS topic name.
 * @param data       Received sample; valid only for the lifetime of this call.
 * @param userData   Opaque pointer supplied at subscription time.
 */
typedef void (*HsilStreamingDataCallback)(const char*             topicName,
                                          const HsilStreamingData* data,
                                          void*                    userData);



/*============================================================================*/
/* LIFECYCLE                                                                   */
/*============================================================================*/

/**
 * @brief Create an HSIL CoSim session on a specific DDS domain (when no 
 *        hsil configuration file is provided).
 *
 * Initializes the underlying DDS participant on the given domain and prepares
 * the session for subsequent subscribe / publish calls.
 *
 * @param domainId      DDS domain identifier [0..232].
 * @param ddsConfigPath Optional path to a CycloneDDS XML configuration file.
 *                      When non-NULL the file path is set as the
 *                      CYCLONEDDS_URI environment variable before the DDS
 *                      participant is created, allowing fine-grained middleware
 *                      tuning (network interfaces, discovery, transport, …).
 *                      Pass NULL to leave the environment unchanged.
 * @param[out] handle   Receives the newly created session handle on success.
 * @return HSIL_OK on success, a negative HSIL_ERR_* code on failure.
 */
HSIL_API int hsil_create(int domainId, const char* ddsConfigPath, HsilHandle* handle);

/**
 * @brief Create an HSIL CoSim session from hsil configuration JSON file.
 *
 * Parses the JSON file, joins the DDS domain declared in it, and pre-creates
 * all DDS writers and readers for the topics listed in the file. Because every
 * entity already exists, applications skip the hsil_create_* calls and use
 * hsil_subscribe_grouped() / hsil_subscribe_streaming() to attach callbacks to
 * subscribed topics and hsil_publish_grouped() / hsil_publish_streaming() to
 * write samples on published topics.
 *
 * @param configPath    Path to an HSIL JSON simulation configuration file.
 * @param ddsConfigPath Optional path to a CycloneDDS XML configuration file.
 *                      When non-NULL the file path is set as the
 *                      CYCLONEDDS_URI environment variable before the DDS
 *                      participant is created.
 *                      Pass NULL to leave the environment unchanged.
 * @param[out] handle   Receives the newly created session handle on success.
 * @return HSIL_OK on success, a negative HSIL_ERR_* code on failure.
 */
HSIL_API int hsil_create_from_config(const char* configPath, const char* ddsConfigPath, HsilHandle* handle);

/**
 * @brief Destroy an HSIL CoSim session and release all associated resources.
 *
 * Deletes all DDS entities created for this session, including every publisher
 * and subscriber. After this call the session handle is invalid; never use it
 * afterwards.
 *
 * @param handle  Session handle obtained from hsil_create() or
 *                hsil_create_from_config(). Passing NULL is a no-op.
 */
HSIL_API void hsil_destroy(HsilHandle handle);

/*============================================================================*/
/* SUBSCRIBE                                                                  */
/*============================================================================*/

/**
 * @brief Register a GroupedData callback for a topic, creating the DDS reader 
 *        if needed.
 *
 * On the first call for a given @p topicName the underlying DDS reader is
 * created automatically (get-or-create). Subsequent calls for the same topic
 * reuse the existing reader and simply add another callback.
 *
 * @param handle     Active session handle.
 * @param topicName  DDS topic name (null-terminated, max. 256 characters).
 * @param qos        QoS for the underlying DDS reader, applied only when the
 *                   reader is first created. Pass NULL to use the defaults.
 * @param groupId    Data-group id filter; pass HSIL_ALL_IDS to receive every
 *                   GroupedData sample regardless of its id.
 * @param callback   Function invoked on each matching sample (must not be NULL).
 * @param userData   Opaque pointer forwarded to every callback invocation.
 * @return HSIL_OK on success, HSIL_ERR_DDS if the reader could not be created,
 *         HSIL_ERR_TOPIC_TYPE_MISMATCH if the topic already exists as streaming,
 *         or another negative HSIL_ERR_*.
 */
HSIL_API int hsil_subscribe_grouped(HsilHandle              handle,
                                    const char*             topicName,
                                    const HsilQos*          qos,
                                    uint32_t                groupId,
                                    HsilGroupedDataCallback callback,
                                    void*                   userData);

/**
 * @brief Register a StreamingData callback for a topic, creating the DDS reader if needed.
 *
 * Streaming counterpart to hsil_subscribe_grouped(). The DDS reader is created
 * on the first call and reused on subsequent calls. If @p protocol is not NULL,
 * only samples whose protocol field matches (byte-exact, up to 8 characters)
 * are forwarded; pass NULL to receive all protocols.
 *
 * @param handle     Active session handle.
 * @param topicName  DDS topic name (null-terminated, max. 256 characters).
 * @param qos        QoS for the underlying DDS reader, applied only when the
 *                   reader is first created. Pass NULL to use the defaults.
 * @param protocol   One of ["generic", "CAN_v1", "Eth_v1", "EthJ_v1"], or NULL for all.
 * @param callback   Function invoked on each matching sample (must not be NULL).
 * @param userData   Opaque pointer forwarded to every callback invocation.
 * @return HSIL_OK on success, HSIL_ERR_DDS if the reader could not be created,
 *         HSIL_ERR_TOPIC_TYPE_MISMATCH if the topic already exists as grouped,
 *         or another negative HSIL_ERR_*.
 */
HSIL_API int hsil_subscribe_streaming(HsilHandle                handle,
                                      const char*               topicName,
                                      const HsilQos*            qos,
                                      const char*               protocol,
                                      HsilStreamingDataCallback callback,
                                      void*                     userData);

/**
 * @brief Query the discovery and QoS compatibility status of a subscriber.
 *
 * Subscriber counterpart to hsil_get_publisher_status().
 *
 * @param handle        Active session handle.
 * @param topicName     Topic whose subscriber is queried.
 * @param[out] status   Receives the status on success (must not be NULL).
 * @return HSIL_OK on success, HSIL_ERR_UNKNOWN_TOPIC if the topic is not known,
 *         HSIL_ERR_SUB_NOT_CREATED if the topic has no subscriber,
 *         or another negative HSIL_ERR_*.
 */
HSIL_API int hsil_get_subscriber_status(HsilHandle  handle,
                                        const char* topicName,
                                        HsilEndpointStatus* status);


/*============================================================================*/
/* PUBLISH                                                                    */
/*============================================================================*/

/**
 * @brief Create a GroupedData publisher (DDS writer) for a named topic.
 * 
 * @note Don't call this function when using publisher is already pre-created for 
 * this topic from the hsil configuration file.
 *
 * Creates the underlying DDS writer for @p topicName so that subsequent
 * hsil_publish_grouped() calls can transmit samples. Calling it again for the
 * same topic succeeds and reuses the existing writer. In config mode the writers
 * are pre-created from the configuration file, so this call is not needed.
 * 
 * Explicit create API for publishers only, to allow users to avoid the overhead 
 * of creating a writer on the first publish call at run-time.
 *
 * @param handle     Active session handle.
 * @param topicName  DDS topic name (null-terminated, max. 256 characters).
 * @param qos        QoS applied only when the writer is first created. Pass NULL
 *                   or a zero-initialised struct to use the defaults (a warning
 *                   is emitted); invalid fields are defaulted likewise.
 * @return HSIL_OK on success, HSIL_ERR_TOPIC_TYPE_MISMATCH if the topic is
 *         already in use as a streaming topic, or another negative HSIL_ERR_*.
 */
HSIL_API int hsil_create_grouped_publisher(HsilHandle     handle,
                                           const char*    topicName,
                                           const HsilQos* qos);

/**
 * @brief Create a StreamingData publisher (DDS writer) for a named topic.
 * 
 * @note Don't call this function when using publisher is already pre-created for 
 * this topic from the hsil configuration file.
 * 
 * Creates the underlying DDS writer for @p topicName so that subsequent
 * hsil_publish_streaming() calls can transmit samples. Calling it again for the
 * same topic succeeds and reuses the existing writer. In config mode the writers
 * are pre-created from the configuration file, so this call is not needed.
 * 
 * Explicit create API for publishers only, to allow users to avoid the overhead 
 * of creating a writer on the first publish call at run-time.
 *
 * @param handle     Active session handle.
 * @param topicName  DDS topic name (null-terminated, max. 256 characters).
 * @param qos        QoS applied only when the writer is first created.
 * @return HSIL_OK on success, HSIL_ERR_TOPIC_TYPE_MISMATCH if the topic is
 *         already in use as a grouped topic, or another negative HSIL_ERR_*.
 */
HSIL_API int hsil_create_streaming_publisher(HsilHandle     handle,
                                             const char*    topicName,
                                             const HsilQos* qos);

/**
 * @brief Query the discovery and QoS compatibility status of a publisher.
 *
 * Reports how many remote subscribers the publisher of @p topicName is matched
 * with, and whether any was rejected for incompatible QoS. Publishing while
 * matchedCount is 0 is not an error, but the samples are discarded. The call
 * does not block; poll it to wait for discovery.
 *
 * @param handle        Active session handle.
 * @param topicName     Topic whose publisher is queried.
 * @param[out] status   Receives the status on success (must not be NULL).
 * @return HSIL_OK on success, HSIL_ERR_UNKNOWN_TOPIC if the topic is not known,
 *         HSIL_ERR_PUB_NOT_CREATED if the topic has no publisher,
 *         or another negative HSIL_ERR_*.
 */
HSIL_API int hsil_get_publisher_status(HsilHandle  handle,
                                       const char* topicName,
                                       HsilEndpointStatus* status);

/**
 * @brief Write a GroupedData sample to the DDS network.
 *
 * The DDS writer must already exist – created with hsil_create_grouped_publisher()
 * or pre-created from the configuration file. The publisher is looked up internally 
 * by @p topicName. The library copies all data referenced by @p data before returning; 
 * the caller may free or reuse its buffers immediately after this call.
 *
 * @param handle     Active session handle.
 * @param topicName  Topic to publish on.
 * @param data       Sample to transmit (must not be NULL).
 * @return HSIL_OK on success, HSIL_ERR_UNKNOWN_TOPIC if no grouped publisher
 *         exists for the topic, HSIL_ERR_TOPIC_TYPE_MISMATCH on a type clash,
 *         or another negative HSIL_ERR_*.
 */
HSIL_API int hsil_publish_grouped(HsilHandle             handle,
                                  const char*            topicName,
                                  const HsilGroupedData* data);

/**
 * @brief Write a StreamingData sample to the DDS network.
 *
 * Streaming counterpart to hsil_publish_grouped(). The library copies all data
 * referenced by @p data before returning.
 *
 * @param handle     Active session handle.
 * @param topicName  Topic to publish on.
 * @param data       Sample to transmit (must not be NULL).
 * @return HSIL_OK on success, HSIL_ERR_UNKNOWN_TOPIC if no streaming publisher
 *         exists for the topic, HSIL_ERR_TOPIC_TYPE_MISMATCH on a type clash,
 *         or another negative HSIL_ERR_*.
 */
HSIL_API int hsil_publish_streaming(HsilHandle               handle,
                                    const char*              topicName,
                                    const HsilStreamingData* data);


/*============================================================================*/
/* CONFIG MODE – SIGNAL ACCESSOR                                              */
/*============================================================================*/

/**
 * @brief Check whether a value of type @p fromType can be assigned to a target of type @p toType.
 *        This function also logs a warning for lossy conversions and an error for
 *        invalid conversions via the internal HSIL logging subsystem.
 * 
 * This is intended for use in config-mode callbacks after reading a signal with hsil_read_signal_by_name() 
 * to check whether the returned value can be safely converted to a different type if needed. 
 * Refer to hsil specification for the conversion rules for more information.
 *
 * Usage example:
 * @code
 * HsilSignalConversion conv = hsil_check_signal_conversion(HSIL_SIGNAL_FLOAT64,
 *                                                          HSIL_SIGNAL_FLOAT32);
 * if (conv == HSIL_SIGNAL_CONV_INVALID) { // handle error }
 * @endcode
 *
 * @param fromType  Source signal type (the type the value is being read from).
 * @param toType    Target signal type (the type the value will be written into).
 * @return @c HSIL_SIGNAL_CONV_OK      if the conversion is lossless.
 *         @c HSIL_SIGNAL_CONV_LOSSY   if precision or range may be lost.
 *         @c HSIL_SIGNAL_CONV_INVALID if the combination is not permitted.
 */
HSIL_API HsilSignalConversion hsil_check_signal_conversion(HsilSignalType fromType, HsilSignalType toType);


/**
 * @brief Query the required size of the @c HsilGroupedData.data buffer (when using hsil configuration file).
 *
 *
 * Usage example:
 * @code
 * size_t sz = 0;
 * if (hsil_get_grouped_data_size(h, 1u, &sz) == HSIL_OK)
 * {
 *     std::vector<uint8_t> buf(sz, 0u);
 *     HsilGroupedData pkt = { .id = 1u, .data = buf.data(), .dataSize = sz };
 *     // … write signals with hsil_write_signal_by_name(), then publish …
 * }
 * @endcode
 *
 * @param handle          Active config-mode session handle.
 * @param groupId         The data-group identifier as declared in the configuration file.
 * @param[out] dataSize   Receives the required buffer size in bytes on success.
 * @return HSIL_OK on success.
 *         HSIL_ERR_INVALID_ARG if @p handle or @p dataSize is NULL.
 *         HSIL_ERR_CONFIG if the session was not created from a configuration file.
 *         HSIL_ERR_INVALID_ARG if @p groupId is not found in the configuration.
 */
HSIL_API int hsil_get_grouped_data_size(HsilHandle handle,
                                        uint32_t   groupId,
                                        size_t*    dataSize);


/**
 * @brief Read one scalar element of a named signal from a GroupedData payload (config mode).
 *
 * Typical use of this API is inside a GroupedData callback to read named signals from the reveived packet.
 * 
 * @note Additionally, the caller can use hsil_check_signal_conversion() to check whether the 
 *       returned value can be safely converted to a different type if needed.
 *
 * Usage example:
 * @code
 * void onData(const char* topic, const HsilGroupedData* pkt, void* ctx)
 * {
 *     HsilSignalValue val;
 *     HsilSignalType  type;
 *     if (hsil_read_signal_by_name(handle, topic, pkt, "vehicle.speed", 0, &val, &type) == HSIL_OK)
 *         printf("speed = %f\n", val.asFloat32);
 * }
 * @endcode
 *
 * @param handle      Active config-mode session handle.
 * @param topicName   DDS topic name the sample was received on.
 * @param data        The received GroupedData sample; @c data->id selects the group.
 * @param signalName  Access point name as declared in the configuration file.
 * @param arrayIndex  Zero-based element index; pass 0 for scalar signals.
 * @param[out] value  Receives the extracted scalar value.
 * @param[out] type   Receives the wire type; determines which union member to read.
 * @return HSIL_OK on success.
 *         HSIL_ERR_INVALID_ARG if any pointer argument is NULL or @p data->data is NULL.
 *         HSIL_ERR_UNKNOWN_TOPIC if @p topicName is not found in the configuration.
 *         HSIL_ERR_UNKNOWN_SIGNAL if @p signalName is not found for the packet's group id.
 *         HSIL_ERR_GENERIC if @p arrayIndex is out of range or the payload is too short.
 */
HSIL_API int hsil_read_signal_by_name(HsilHandle             handle,
                                      const char*            topicName,
                                      const HsilGroupedData* data,
                                      const char*            signalName,
                                      size_t                 arrayIndex,
                                      HsilSignalValue*       value,
                                      HsilSignalType*        type);


/**
 * @brief Write one scalar element of a named signal into a GroupedData payload (config mode).
 *
 * After updating all required signals the caller should invoke
 * hsil_publish_grouped() to transmit the completed packet.
 *
 * Usage example:
 * @code
 * uint8_t buf[12] = {};
 * HsilGroupedData pkt = { .id = 1, .data = buf, .dataSize = sizeof(buf) };
 *
 * HsilSignalValue val;
 * val.asFloat32 = 42.5f;
 * int rc = hsil_write_signal_by_name(handle, "VehicleSignals", &pkt,
 *                                    "vehicle.speed", 0, val, HSIL_SIGNAL_FLOAT32);
 * // rc == HSIL_OK; buf now contains 42.5f at bytes [0..3]
 * // Write other signals into the packet as needed, then publish:
 * hsil_publish_grouped(handle, "VehicleSignals", &pkt);
 * @endcode
 *
 * @param handle      Active config-mode session handle.
 * @param topicName   DDS topic name the sample will be published on.
 * @param data        GroupedData packet whose @c data buffer will be modified.
 *                    @c data->id selects the group; @c data->data must not be NULL
 *                    and @c data->dataSize must cover the signal's byte range.
 * @param signalName  Access point name as declared in the configuration file.
 * @param arrayIndex  Zero-based element index; pass 0 for scalar signals.
 * @param value       Value to write; the relevant union member must be set.
 * @param type        Wire type of @p value; used to check conversion compatibility.
 * @return HSIL_OK on success.
 *         HSIL_WARN_SIGNAL_CONVERSION if the write succeeded but @p type is lossy
 *             relative to the configured wire type.
 *         HSIL_ERR_INVALID_ARG if any pointer argument is NULL or @p data->data is NULL.
 *         HSIL_ERR_UNKNOWN_TOPIC if @p topicName is not found in the configuration.
 *         HSIL_ERR_UNKNOWN_SIGNAL if @p signalName is not found for the packet's group id.
 *         HSIL_ERR_SIGNAL_CONVERSION if the type combination is invalid.
 *         HSIL_ERR_GENERIC if @p arrayIndex is out of range or the payload is too short.
 */
HSIL_API int hsil_write_signal_by_name(HsilHandle             handle,
                                       const char*            topicName,
                                       HsilGroupedData*       data,
                                       const char*            signalName,
                                       size_t                 arrayIndex,
                                       const HsilSignalValue  value,
                                       const HsilSignalType   type);


#ifdef __cplusplus
}
#endif

#endif /* HSIL_COSIM_H */
