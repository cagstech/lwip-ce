
#include <usbdrvce.h>
#include <ti/getcsc.h>
#include <ti/screen.h>

#include "lwip/init.h"
#include "lwip/sys.h"
#include "lwip/timeouts.h"
#include "lwip/netif.h"

/* These commented out, but may be headers you might wish to enable. */
// #include "lwip/altcp_tcp.h"
// #include "lwip/altcp.h"
// #include "lwip/udp.h"
#include "lwip/dhcp.h"
// #include "lwip/dns.h"
#include "lwip/apps/httpd.h"
// due to the build structure of lwIP, "lwip/file.h" corresponds to "include/lwip/file.h"

#include "drivers/usb-ethernet.h"

void ethif_status_callback_fn(struct netif *netif)
{
    printf("%s\n", ip4addr_ntoa(netif_ip4_addr(netif)));
}

int main(void)
{
    lwip_init();
    struct netif *ethif = NULL;
    os_ClrHomeFull();

    /* You should probably handle this function failing */
    if (usb_Init(eth_handle_usb_event, NULL, NULL, USB_DEFAULT_INIT_FLAGS))
        goto exit;

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
                printf("netif found\n");
                netif_set_status_callback(ethif, ethif_status_callback_fn);
                dhcp_start(ethif);
                httpd_init();
                printf("httpd running\n");
            }
        }
        usb_HandleEvents();            // usb events
        sys_check_timeouts();          // lwIP timers/event callbacks
    } while (os_GetCSC() != sk_Clear); // this should contain an actual loop-ending condition
exit:
    usb_Cleanup();
    exit(0);
}
