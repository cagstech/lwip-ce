
#include <usbdrvce.h>

#include "lwip/init.h"
#include "lwip/timeouts.h"
#include "lwip/netif.h"

/* These commented out, but may be headers you might wish to enable. */
// #include "lwip/altcp_tcp.h"
// #include "lwip/altcp.h"
// #include "lwip/udp.h"
// #include "lwip/dhcp.h"
// #include "lwip/dns.h"
// due to the build structure of lwIP, "lwip/file.h" corresponds to "include/lwip/file.h"

#include "drivers/cdc.h"

int main(void)
{
    lwip_init();
    struct netif *ethif = NULL;

    /* You should probably handle this function failing */
    usb_Init(eth_handle_usb_event, NULL, NULL, USB_DEFAULT_INIT_FLAGS);

    do
    {
        // this is your code that runs in a loop
        // please note that much of the networking in lwIP is callback-style
        // please consult the lwIP documentation for the protocol you are using for instructions

        if (ethif == NULL)
        {
            // because the netif initialization is done in the ECM/NCM driver, you will need to poll for the presence of
            // the desired netif.
            // perhaps check for netif's by IF name in a loop from en0 to en9.
            ethif = netif_find("en0");
            if (ethif)
            {
                // run this code if netif exists
                // eg: dhcp_start(ethif);
                printf("found netif\n");
            }
        }
        else
        {
            // execute this code if netif exists
        }

        usb_HandleEvents();   // usb events
        sys_check_timeouts(); // lwIP timers/event callbacks
    } while (/* exit condition */);
    usb_Cleanup();
    exit(0);
}
