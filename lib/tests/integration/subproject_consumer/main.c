// SPDX-FileCopyrightText: 2026 dSPACE SE & Co. KG
// SPDX-License-Identifier: Apache-2.0

/** <!-------------------------------------------------------------------------->
*
*   @file main.c
*
*   @brief Subproject-consumer integration test for HsilCoSim.
*
*   @author
*       dSPACE SE & Co. KG
*
*   @description
*       Verifies that an application can consume HsilCoSim through
*       add_subdirectory() and initialize a session.
*
*   <hr><br>
*<!-------------------------------------------------------------------------->*/

#include <hsil/hsil.h>
#include <stdio.h>

int main(void)
{
    printf("[SubprojectConsumer] Initializing HsilCoSim on domain 109...\n");

    HsilHandle handle = NULL;
    int rc = hsil_create(109, NULL, &handle);
    if (rc != HSIL_OK)
    {
        fprintf(stderr, "[SubprojectConsumer] FAILED: hsil_create returned error %d\n", rc);
        return 1;
    }

    if (handle == NULL)
    {
        fprintf(stderr, "[SubprojectConsumer] FAILED: returned handle is NULL\n");
        return 1;
    }

    printf("[SubprojectConsumer] Successfully created handle %p. Destroying...\n", (void*)handle);
    hsil_destroy(handle);

    printf("[SubprojectConsumer] SUCCESS: Subproject integration verified.\n");
    return 0;
}
