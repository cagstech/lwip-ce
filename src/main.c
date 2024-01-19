#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <ti/screen.h>
#undef NDEBUG
// #include <debug.h>

#include <keypadc.h>
#include <usbdrvce.h>
#include "include/lwip/init.h"
#include "include/lwip/netif.h"
#include "include/lwip/tcp.h"
#include "include/lwip/udp.h"
#include "include/lwip/timeouts.h"
#include "include/lwip/sys.h"
#include "include/lwip/snmp.h"
#include "include/lwip/pbuf.h"
#include "include/lwip/dhcp.h"
#include "ecm.h"

struct udp_pcb *pcb;

int main(void)
{
    os_ClrLCDFull();
    // initialize usb hardware
    lwip_init();
    pbuf_init();
    udp_init();
    pcb = udp_new();
    if (usb_Init(ecm_handle_usb_event, NULL, NULL /* descriptors */, USB_DEFAULT_INIT_FLAGS))
        return 1;

    // wait for ecm device to be ready
    bool ip_setup_done = false;

    kb_SetMode(MODE_3_CONTINUOUS);
    do
    {
        if (kb_IsDown(kb_KeyAlpha))
        {
            if (dhcp_supplied_address(&ecm_device.netif))
                printf("dhcp_addr %s\n", ip4addr_ntoa(netif_ip4_addr(&ecm_device.netif)));
            else
                printf("no ip configd\n");
        }
        if (kb_IsDown(kb_Key2nd))
        {
            static const char *text1 = "The fox jumped over the dog.";
            ip4_addr_t sendto = IPADDR4_INIT_BYTES(192, 168, 1, 216);
            struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, strlen(text1), PBUF_RAM);
            if (p)
            {
                pbuf_take(p, text1, strlen(text1));
                printf("%i\n", udp_sendto(pcb, p, &sendto, 51000));
            }
            else
            {
                printf("buf alloc err\n");
                pbuf_free(p);
            }
        }
        if (kb_IsDown(kb_KeyClear))
            break;
        sys_check_timeouts();
        usb_HandleEvents();
    } while (1);
    dhcp_stop(&ecm_device.netif);
    usb_Cleanup();
    /* Start DHCP and HTTPD */
    //

    // todo: actually check for this
    // netif_set_link_up(&netif);

    // kb_SetMode(MODE_3_CONTINUOUS);
}
