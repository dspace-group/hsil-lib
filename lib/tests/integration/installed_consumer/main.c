/**
 * @file main.c
 * @brief Integration test verifying consumption of HsilCoSim from the install tree.
 */

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
