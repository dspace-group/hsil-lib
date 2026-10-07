// SPDX-FileCopyrightText: 2026 dSPACE SE & Co. KG
// SPDX-License-Identifier: Apache-2.0

/** <!-------------------------------------------------------------------------->
*
*   @file main.c
*
*   @brief Installed-consumer integration test for HsilCoSim.
*
*   @author
*       dSPACE SE & Co. KG
*
*   @description
*       Verifies that an application can consume HsilCoSim from an installed
*       package and initialize a session.
*
*   <hr><br>
*<!-------------------------------------------------------------------------->*/

#include <hsil/hsil.h>
#include <stdio.h>

int main(void)
{
    printf("[InstalledConsumer] Initializing HsilCoSim from install tree on domain 110...\n");

    HsilHandle handle = NULL;
    int rc = hsil_create(110, NULL, &handle);
    if (rc != HSIL_OK)
    {
        fprintf(stderr, "[InstalledConsumer] FAILED: hsil_create returned error %d\n", rc);
        return 1;
    }

    if (handle == NULL)
    {
        fprintf(stderr, "[InstalledConsumer] FAILED: returned handle is NULL\n");
        return 1;
    }

    printf("[InstalledConsumer] Successfully created handle %p. Destroying...\n", (void*)handle);
    hsil_destroy(handle);

    printf("[InstalledConsumer] SUCCESS: Installed package integration verified.\n");
    return 0;
}
