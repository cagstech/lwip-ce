#include <ti/screen.h>
#include <ti/getkey.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "tls/includes/random.h"

/* Main function, called first */
int main(void)
{
    /* Clear the homescreen */
    os_ClrHome();
    
    bool tls_random_inited = false;
    for(int i=0; i<5; i++)  // run a few times bc cemu isn't truly random
        tls_random_inited |= tls_random_init_entropy();
    
    if(tls_random_inited)
        printf("success");
    else printf("failed");
    os_GetKey();
    return 0;
}
