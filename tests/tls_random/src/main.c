#include <ti/screen.h>
#include <ti/getkey.h>
#include <sys/timers.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "tls/includes/random.h"

/* Main function, called first */
int main(void)
{
    /* Clear the homescreen */
    os_ClrHome();
    bool rand_inited = false;
    uint24_t retries=100;
    do {
        rand_inited |= tls_random_init_entropy();
        boot_WaitShort();
    } while(retries && (!rand_inited));
    
    if(rand_inited)
        printf("success");
    else printf("failed");
    os_GetKey();
    return 0;
}
