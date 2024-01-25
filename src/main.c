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

// struct tcp_pcb pcb;
struct tcp_pcb_listen pcbl;

#define CHAT_MAX_CONNS 10
struct _conn
{
    ip_addr_t user_ip;
    struct tcp_pcb *user_pcb;

} struct _conn connections[CHAT_MAX_CONNS];

void broadcast(void *data)
{
    printf("%s\n", data);
    for (int i = 0; i < CHAT_MAX_CONNS; i++)
    {
        tcp_write(connections[i].user_pcb, data, strlen(data) + 1, 0);
        tcp_output(connections[i].user_pcb);
    }
}

err_t tcp_accept_conn(void *arg, struct tcp_pcb *newpcb, err_t err)
{
    broacast("new client from %s\n", ipaddr_ntoa(newpcb->localip));
}

int main(void)
{
    os_ClrHome();
    gfx_Begin();
    newline();
    gfx_SetTextXY(2, LCD_HEIGHT - 10);
    lwip_init();
    printf("lwIP public beta 1\n");
    printf("Simple IRC connect\n");
    pcbl = tcp_new();
    if (usb_Init(ecm_handle_usb_event, NULL, NULL /* descriptors */, USB_DEFAULT_INIT_FLAGS))
        return 1;

    // wait for ecm device to be ready
    bool tcp_listener_up = false;

    kb_SetMode(MODE_3_CONTINUOUS);
    do
    {
        if (dhcp_supplied_address(&ecm_netif) && (!tcp_listener_up))
        {
            printf("dhcp_addr %s\n", ip4addr_ntoa(netif_ip4_addr(&ecm_netif)));
            printf("enabling tcp listener\n");
            tcp_accept(&pcbl, tcp_accept_conn);
            tcp_listen(&pcbl);
        }
        if (kb_IsDown(kb_Key2nd))
        {
            static const char *text1 = "The fox jumped over the dog.";
            ip_addr_t sendto = IPADDR4_INIT_BYTES(192, 168, 1, 219);
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
