#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <ti/screen.h>
#include <sys/timers.h>
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

struct tcp_pcb *pcb;
ip_addr_t remote_ip = IPADDR4_INIT_BYTES(192, 168, 1, 219);

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

void dns_lookup_callback(const char *name, const ip_addr_t *ipaddr, void *callback_arg)
{
    if (ipaddr)
        memcpy(&remote_ip, ipaddr, sizeof(ipaddr));
}

err_t tcp_connect_callback(void *arg, struct tcp_pcb *tpcb, err_t err)
{
    printf("connected to server\n");
}

#define CHAT_MAX_CONNS 10

int main(void)
{
    os_ClrHome();
    gfx_Begin();
    newline();
    gfx_SetTextXY(2, LCD_HEIGHT - 10);
    printf("lwIP public beta 1\n");
    printf("Simple IRC connect\n");
    lwip_init();
    if (usb_Init(ecm_handle_usb_event, NULL, NULL /* descriptors */, USB_DEFAULT_INIT_FLAGS))
        return 1;

    // wait for ecm device to be ready
    pcb = tcp_new();
    bool tcp_listener_up = false;

    kb_SetMode(MODE_3_CONTINUOUS);
    do
    {
        // printf("%lu\n", sys_now());
        if (dhcp_supplied_address(&ecm_netif) && (!tcp_listener_up))
        {
            printf("dhcp_addr %s\n", ip4addr_ntoa(netif_ip4_addr(&ecm_netif)));
            // printf("dns query for remote\n");
            // dns_gethostbyname("acagliano.ddns.net", &remote_ip, dns_lookup_callback, NULL);
            // printf("enabling tcp listener\n");
            // tcp_bind(pcb, IP4_ADDR_ANY, 0);
            tcp_connect(pcb, &remote_ip, 8881, tcp_connect_callback);
            tcp_listener_up = true;
        }
        if (kb_IsDown(kb_Key2nd))
        {
            static const char *text1 = "The fox jumped over the dog.";
            tcp_write(pcb, text1, strlen(text1) + 1, TCP_WRITE_FLAG_COPY);
            tcp_output(pcb);
        }
        if (kb_IsDown(kb_KeyClear))
            break;
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
