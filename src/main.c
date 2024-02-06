#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <ti/screen.h>
#include <sys/timers.h>
#undef NDEBUG
// #include <debug.h>
// #define ENABLE_VETH 1

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

#ifdef ENABLE_VETH
#include "veth.h"
#endif
// #include "veth.h"

struct tcp_pcb *pcb, *cpcb;
ip_addr_t remote_ip = IPADDR4_INIT_BYTES(192, 168, 1, 24);

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

void tcp_error_callback(void *arg, err_t err)
{
    struct tcp_pcb *pcb = (struct tcp_pcb *)arg;

    if (err == ERR_OK)
    {
        // No error, do nothing
        return;
    }

    // Handle the TCP error
    if (err == ERR_ABRT)
    {
        // Connection aborted
        printf("tcp: connection aborted\n");
    }
    else if (err == ERR_RST)
    {
        // Connection reset by the peer
        printf("tcp: connection reset by peer\n");
    }
    else
    {
        // Other TCP errors
        printf("tcp: error - %s\n", lwip_strerr(err));
    }

    // Optionally, perform cleanup or take appropriate action
    // ...

    // Close the TCP connection (if not already closed)
    if (pcb != NULL)
    {
        tcp_arg(pcb, NULL);
        tcp_sent(pcb, NULL);
        tcp_recv(pcb, NULL);
        tcp_err(pcb, NULL);
        tcp_close(pcb);
    }
}

err_t tcp_recv_callback(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err)
{
    LWIP_UNUSED_ARG(arg);
    if (err == ERR_OK && p != NULL)
    {
        // Data received successfully
        printf("tcp: received, length = %d\n", p->tot_len);

        // Process or handle the received data
        // process_received_data(p);
        tcp_recved(tpcb, p->tot_len);
        pbuf_free(p);
    }

    else if (err == ERR_OK && p == NULL)
    {
        // The remote side has closed the connection
        printf("tcp: connection closed by remote\n");
        // Optionally, perform cleanup or take appropriate action
    }
    else
    {
        // Handle receive error
        printf("tcp: receive error - %s\n", lwip_strerr(err));
    }

    return ERR_OK;
}

err_t tcp_sent_callback(void *arg, struct tcp_pcb *tpcb, u16_t len)
{
    LWIP_UNUSED_ARG(arg);
    LWIP_UNUSED_ARG(tpcb);
    // Data has been successfully sent
    printf("tcp: sent, length = %d\n", len);

    // Optionally, perform additional actions after successful send

    return ERR_OK;
}

err_t tcp_connect_callback(void *arg, struct tcp_pcb *tpcb, err_t err)
{
    LWIP_UNUSED_ARG(arg);
    if (err == ERR_OK)
    {
        // Connection successfully established
        printf("tcp: connected\n");
        tcp_sent(tpcb, tcp_sent_callback);
        tcp_recv(tpcb, tcp_recv_callback);
        tcp_err(tpcb, tcp_error_callback);
    }
    else
    {
        // Handle connection error
        printf("tcp: connection error - %s\n", lwip_strerr(err));
    }

    // You can use the 'arg' parameter to pass additional context or data.
    // In this example, we're not using it.

    return ERR_OK;
}

#define CHAT_MAX_CONNS 10

int main(void)
{
    const char *text1 = "The fox jumped over the dog.";
    struct netif vethif;
    os_ClrHome();
    gfx_Begin();
    newline();
    gfx_SetTextXY(2, LCD_HEIGHT - 10);
    printf("lwIP public beta 1\n");
    printf("Simple IRC connect\n");
    lwip_init();
    if (usb_Init(ecm_handle_usb_event, NULL, NULL /* descriptors */, USB_DEFAULT_INIT_FLAGS))
        return 1;

#ifdef ENABLE_VETH
    netif_add_noaddr(&vethif, NULL, vethif_init, netif_input);
    netif.name[0] = 'e';
    netif.name[1] = 'n';
    netif.num = 1;
    netif_create_ip6_linklocal_address(&netif, 1);
    netif.ip6_autoconfig_enabled = 1;
    netif_set_status_callback(&netif, netif_status_callback);
    netif_set_up(&netif);
#endif
    netif_set_default(&netif);
    // wait for ecm device to be ready
    bool tcp_connected = false;

    kb_SetMode(MODE_3_CONTINUOUS);
    do
    {
        // printf("%lu\n", sys_now());
        if (dhcp_supplied_address(&ecm_netif) && (!tcp_connected))
        {
            printf("dhcp_addr %s\n", ip4addr_ntoa(netif_ip4_addr(&ecm_netif)));
            // printf("dns query for remote\n");
            // dns_gethostbyname("acagliano.ddns.net", &remote_ip, dns_lookup_callback, NULL);
            // printf("enabling tcp listener\n");
            // tcp_bind(pcb, IP4_ADDR_ANY, 0);
            pcb = tcp_new();
            if (pcb == NULL)
                return 2;
            tcp_arg(pcb, pcb);
            if (tcp_connect(pcb, &remote_ip, 8881, tcp_connect_callback))
            {
                printf("tcp: connect error\n");
                break;
            }
            tcp_connected = true;
        }
        if (kb_IsDown(kb_Key2nd))
        {
            printf("sending: %s\n", text1);
            if (tcp_write(pcb, text1, strlen(text1), 0) == ERR_OK)
                tcp_output(pcb);
            while (kb_IsDown(kb_Key2nd))
                ;
        }
        if (kb_IsDown(kb_KeyClear))
            break;
        usb_HandleEvents();
        sys_check_timeouts();
    } while (1);
    tcp_abort(pcb);
    dhcp_stop(&ecm_netif);
    usb_Cleanup();
    gfx_End();

    // todo: actually check for this
    // netif_set_link_up(&netif);

    // kb_SetMode(MODE_3_CONTINUOUS);
}
