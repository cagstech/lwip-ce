
#include <usbdrvce.h>
#include <ti/getcsc.h>
#include <ti/screen.h>

#include "lwip/init.h"
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

bool run_main = false;
bool httpd_running = false;

void ethif_status_callback_fn(struct netif *netif)
{
    if (dhcp_supplied_address(netif) && (!httpd_running))
    {
        httpd_init();
        printf("httpd listen on %s\n", ip4addr_ntoa(netif_ip4_addr(netif)));
        httpd_running = true;
    }
}

int main(void)
{
    uint8_t key;
    lwip_init();
    struct netif *ethif = NULL;
    os_ClrHomeFull();

    /* You should probably handle this function failing */
    if (usb_Init(eth_handle_usb_event, NULL, NULL, USB_DEFAULT_INIT_FLAGS))
        goto exit;

    run_main = true;

    do
    {
        // this is your code that runs in a loop
        // please note that much of the networking in lwIP is callback-style
        // please consult the lwIP documentation for the protocol you are using for instructions
        os_ClrHomeFull();
        key = os_GetCSC();
        if (key == sk_Clear)
        {
            run_main = false;
        }
        if (ethif == NULL)
        {
            if ((ethif = netif_find("en0")))
            {
                // run this code if netif exists
                // eg: dhcp_start(ethif);
                printf("en0 registered\n");
                netif_set_status_callback(ethif, ethif_status_callback_fn);
            }
        }
        printf("handling events\n");
        usb_HandleEvents(); // usb events
        printf("done\n");
        sys_check_timeouts(); // lwIP timers/event callbacks
    } while (run_main);
exit:
    usb_Cleanup();
    exit(0);
}
