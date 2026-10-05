/** <!-------------------------------------------------------------------------->
*
*   @file cycloneDdsProvider.h
*
*   @brief CycloneDDS implementation of the HSIL DDS abstraction layer.
*
*   @author
*       dSPACE SE & Co. KG
*
*   @description
*       Declares hsil_getDdsOps() for the CycloneDDS backend. This header is
*       only included internally; consumers of the HSIL library use hsil.h.
*
*   @copyright
*       Copyright 2026, dSPACE SE & Co. KG. All rights reserved.
*
*   <hr><br>
*<!-------------------------------------------------------------------------->*/

#pragma once

#include "ddsAbstraction.h"

/**
 * @brief Return the CycloneDDS implementation of the HsilDdsOps vtable.
 *
 * This function satisfies the contract declared in ddsAbstraction.h.
 * It is the single entry-point that links the main library to this vendor
 * backend; no other symbols from this translation unit are exposed.
 *
 * @return Pointer to a statically allocated, fully populated HsilDdsOps table.
 */
const HsilDdsOps* hsil_getDdsOps(void);
