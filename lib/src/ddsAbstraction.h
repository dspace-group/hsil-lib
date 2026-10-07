// SPDX-FileCopyrightText: 2026 dSPACE SE & Co. KG
// SPDX-License-Identifier: Apache-2.0

/** <!-------------------------------------------------------------------------->
*
*   @file ddsAbstraction.h
*
*   @brief Internal DDS vendor abstraction layer (vtable).
*
*   @author
*       dSPACE SE & Co. KG
*
*   @description
*       Defines the HsilDdsOps function-pointer table that every DDS vendor
*       backend must implement. The main library interacts with DDS exclusively
*       through this interface, keeping vendor-specific code isolated in the
*       src/vendors/ subdirectories.
*
*   <hr><br>
*<!-------------------------------------------------------------------------->*/

#pragma once

#include <hsil/hsil.h>

/*----------------------------------------------------------------------------*/
/* DDS OPS VTABLE                                                             */
/*----------------------------------------------------------------------------*/

/**
 * @brief Function-pointer table (vtable) implemented by each DDS vendor backend.
 *
 * Each vendor supplies exactly one static instance of this structure and
 * returns a pointer to it from hsil_getDdsOps(). The main library resolves all
 * DDS operations through this table, so swapping vendors requires only
 * relinking against a different backend.
 *
 * All functions receive an opaque @p ctx pointer that was returned by init().
 * Functions may assume @p ctx is non-NULL unless documentation states otherwise.
 */
struct HsilDdsOps
{
    /**
     * @brief Initialize the DDS stack and create a DDS participant.
     *
     * @param domainId      DDS domain identifier [0..232].
     * @param ddsConfigPath Optional path to a CycloneDDS XML configuration file.
     *                      When non-NULL the implementation sets CYCLONEDDS_URI
     *                      to this path before creating the participant.
     *                      Pass NULL to leave the environment unchanged.
     * @return Opaque vendor context pointer on success, NULL on failure.
     */
    void* (*init)(int domainId, const char* ddsConfigPath);

    /**
     * @brief Tear down the DDS stack and free all resources.
     *
     * Called once when the session is destroyed. After this call the context
     * pointer is invalid and must not be used again.
     *
     * @param ctx  Vendor context from init().
     */
    void (*destroy)(void* ctx);

    /* ------------------------------------------------------------------ */
    /* Writers                                                              */
    /* ------------------------------------------------------------------ */

    /**
     * @brief Create a DDS DataWriter for GroupedData on the given topic.
     *
     * @param ctx        Vendor context.
     * @param topicName  DDS topic name (null-terminated).
     * @param qos        QoS settings to apply to the writer; never NULL
     *                   (the caller has already validated and defaulted the struct).
     * @return Opaque writer handle on success, NULL on failure.
     */
    void* (*createGroupedWriter)(void* ctx, const char* topicName, const HsilQos* qos);

    /**
     * @brief Create a DDS DataWriter for StreamingData on the given topic.
     *
     * @param ctx        Vendor context.
     * @param topicName  DDS topic name (null-terminated).
     * @param qos        QoS settings to apply to the writer; never NULL.
     * @return Opaque writer handle on success, NULL on failure.
     */
    void* (*createStreamingWriter)(void* ctx, const char* topicName, const HsilQos* qos);

    /**
     * @brief Destroy a writer handle previously returned by createGroupedWriter()
     *        or createStreamingWriter().
     *
     * @param ctx     Vendor context.
     * @param writer  Writer handle to destroy.
     */
    void (*destroyWriter)(void* ctx, void* writer);

    /**
     * @brief Write one GroupedData sample.
     *
     * The implementation must copy any payload bytes it needs before returning.
     *
     * @param ctx     Vendor context.
     * @param writer  Writer handle from createGroupedWriter().
     * @param sample  Sample to transmit.
     * @return 0 on success, negative on failure.
     */
    int (*writeGrouped)(void* ctx, void* writer, const HsilGroupedData* sample);

    /**
     * @brief Write one StreamingData sample.
     *
     * @param ctx     Vendor context.
     * @param writer  Writer handle from createStreamingWriter().
     * @param sample  Sample to transmit.
     * @return 0 on success, negative on failure.
     */
    int (*writeStreaming)(void* ctx, void* writer, const HsilStreamingData* sample);

    /* ------------------------------------------------------------------ */
    /* Readers                                                              */
    /* ------------------------------------------------------------------ */

    /**
     * @brief Create a DDS DataReader for GroupedData on the given topic.
     *
     * The reader must invoke @p callback from its data-available listener
     * for every valid received sample, passing @p callbackData as the
     * userData argument.
     *
     * @param ctx           Vendor context.
     * @param topicName     DDS topic name (null-terminated).
     * @param callback      Dispatch function called for each sample.
     * @param callbackData  Passed verbatim as the userData argument to callback.
     * @param qos           QoS settings to apply to the reader; never NULL.
     * @return Opaque reader handle on success, NULL on failure.
     */
    void* (*createGroupedReader)(void*                   ctx,
                                  const char*             topicName,
                                  const HsilQos*          qos,
                                  HsilGroupedDataCallback callback,
                                  void*                   callbackData);

    /**
     * @brief Create a DDS DataReader for StreamingData on the given topic.
     *
     * @param ctx           Vendor context.
     * @param topicName     DDS topic name (null-terminated).
     * @param callback      Dispatch function called for each sample.
     * @param callbackData  Passed verbatim as the userData argument to callback.
     * @param qos           QoS settings to apply to the reader; never NULL.
     * @return Opaque reader handle on success, NULL on failure.
     */
    void* (*createStreamingReader)(void*                     ctx,
                                    const char*               topicName,
                                    const HsilQos*            qos,
                                    HsilStreamingDataCallback callback,
                                    void*                     callbackData);

    /**
     * @brief Destroy a reader handle previously returned by createGroupedReader()
     *        or createStreamingReader().
     *
     * @param ctx     Vendor context.
     * @param reader  Reader handle to destroy.
     */
    void (*destroyReader)(void* ctx, void* reader);

    /**
     * @brief Query the match and QoS compatibility status of a single endpoint.
     *
     * @param ctx           Vendor context.
     * @param endpoint      Writer handle when @p isWriter is non-zero, otherwise
     *                      a reader handle; never NULL.
     * @param isWriter      Non-zero if @p endpoint is a writer, zero for a reader.
     * @param[out] status   Receives the endpoint status; never NULL.
     * @return 0 on success, non-zero on failure.
     */
    int (*getEndpointStatus)(void* ctx, void* endpoint, int isWriter, HsilEndpointStatus* status);
};

/*----------------------------------------------------------------------------*/
/* VENDOR REGISTRATION FUNCTION                                                */
/*----------------------------------------------------------------------------*/

/**
 * @brief Return the DDS operations vtable for the compiled-in vendor backend.
 *
 * Each vendor implements this function in its own translation unit. Only one
 * vendor is linked into a given binary. Adding a new vendor means creating a
 * new subdirectory under src/vendors/ and implementing this function.
 *
 * @return Pointer to a statically allocated HsilDdsOps table; never NULL.
 */
const HsilDdsOps* hsil_getDdsOps(void);
