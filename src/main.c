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
// #include "veth.h"

static void newline(void)
{
    memmove(gfx_vram, gfx_vram + (LCD_WIDTH * 10), LCD_WIDTH * (LCD_HEIGHT - 10));
    gfx_SetColor(255);
    gfx_FillRectangle_NoClip(0, LCD_HEIGHT - 10, LCD_WIDTH, 10);
    gfx_SetTextXY(2, LCD_HEIGHT - 10);
}
void outchar(char c)
{
    if (c == '\n')
    {
        newline();
    }
    else if (c < ' ' || c > '~')
    {
        return;
    }
    else
    {
        if (gfx_GetTextX() >= LCD_WIDTH - 8)
        {
            newline();
        }
        gfx_PrintChar(c);
    }
}

struct udp_pcb *pcb;
/*
static err_t
veth_netif_init(struct netif *netif)
{
    netif->linkoutput = veth_transmit;
    netif->output = etharp_output;
    netif->output_ip6 = ethip6_output;
    netif->mtu = ETHERNET_MTU;
    netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_ETHERNET | NETIF_FLAG_IGMP | NETIF_FLAG_MLD6;
    MIB2_INIT_NETIF(netif, snmp_ifType_ethernet_csmacd, 100000000);
    memcpy(netif->hwaddr, mac_addr, ETH_HWADDR_LEN);
    netif->hwaddr_len = ETH_HWADDR_LEN;
    return ERR_OK;
}
*/
int main(void)
{
    os_ClrHome();
    gfx_Begin();
    newline();
    gfx_SetTextXY(2, LCD_HEIGHT - 10);
    lwip_init();
    // initialize usb hardware
    struct udp_pcb *pcb = udp_new();
    if (usb_Init(ecm_handle_usb_event, NULL, NULL /* descriptors */, USB_DEFAULT_INIT_FLAGS))
        return 1;

    // wait for ecm device to be ready
    bool ip_setup_done = false;

    kb_SetMode(MODE_3_CONTINUOUS);
    do
    {
        if (kb_IsDown(kb_KeyAlpha))
        {
            if (dhcp_supplied_address(&ecm_netif))
                printf("dhcp_addr %s\n", ip4addr_ntoa(netif_ip4_addr(&ecm_netif)));
            else
                printf("no ip configd\n");
        }
        if (kb_IsDown(kb_Key2nd))
        {
            static const char *text1 = "The fox jumped over the dog.";
            ip_addr_t sendto = IPADDR4_INIT_BYTES(192, 168, 2, 3);
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
    dhcp_stop(&ecm_netif);
    usb_Cleanup();
    gfx_End();
    /* Start DHCP and HTTPD */
    //

    // todo: actually check for this
    // netif_set_link_up(&netif);

    // kb_SetMode(MODE_3_CONTINUOUS);
}
