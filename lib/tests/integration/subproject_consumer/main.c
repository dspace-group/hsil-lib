/**
 * @file main.c
 * @brief Integration test verifying consumption of HsilCoSim via add_subdirectory().
 */

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
