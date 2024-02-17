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

#include "drivers.h"

enum
{
    INPUT_LOWER,
    INPUT_UPPER,
    INPUT_NUMBER
};

struct tcp_pcb *pcb, *cpcb;
#ifdef ENABLE_VETH
ip_addr_t remote_ip = IPADDR4_INIT_BYTES(192, 168, 2, 3);
#else
ip_addr_t remote_ip = IPADDR4_INIT_BYTES(192, 168, 1, 24);
#endif

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
        printf("%s\n", p->payload);

        // Process or handle the received data
        // process_received_data(p);
        tcp_recved(tpcb, p->tot_len);
        pbuf_free(p);
        return ERR_OK;
    }

    else if (err == ERR_OK && p == NULL)
    {
        // The remote side has closed the connection
        printf("tcp: connection closed by remote\n");
        // Optionally, perform cleanup or take appropriate action
        tcp_close(pcb);
        return ERR_ABRT;
    }
    else
    {
        // Handle receive error
        printf("tcp: receive error - %s\n", lwip_strerr(err));
        return err;
    }
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

const kb_lkey_t keys_alpha[] = {kb_KeyMath, kb_KeyApps, kb_KeyPrgm, kb_KeyRecip, kb_KeySin, kb_KeyCos, kb_KeyTan, kb_KeyPower, kb_KeySquare, kb_KeyComma, kb_KeyLParen, kb_KeyRParen, kb_KeyDiv, kb_KeyLog, kb_Key7, kb_Key8, kb_Key9, kb_KeyMul, kb_KeyLn, kb_Key4, kb_Key5, kb_Key6, kb_KeySub, kb_KeySto, kb_Key1, kb_Key2, kb_KeyDecPnt};
const kb_lkey_t keys_num[] = {kb_Key0, kb_Key1, kb_Key2, kb_Key3, kb_Key4, kb_Key5, kb_Key6, kb_Key7, kb_Key8, kb_Key9, kb_KeyDecPnt};

const char lc_chars[] = "abcdefghijklmnopqrstuvwxyz.";
const char uc_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ.";
const char num_chars[] = "0123456789.";

struct keyconfig
{
    size_t len;
    kb_lkey_t *active_keys;
    char *active_chars;
};

int main(void)
{
    const char *text1 = "The fox jumped over the dog.";
    struct keyconfig keyconfig[3] =
        {
            {sizeof(keys_alpha), keys_alpha, lc_chars},
            {sizeof(keys_alpha), keys_alpha, uc_chars},
            {sizeof(keys_num), keys_num, num_chars}};

#ifdef ENABLE_VETH
    ip4_addr_t addr = IPADDR4_INIT_BYTES(192, 168, 2, 2);
    ip4_addr_t netmask = IPADDR4_INIT_BYTES(255, 255, 255, 0);
    ip4_addr_t gateway = IPADDR4_INIT_BYTES(192, 168, 2, 1);
#endif
    os_ClrHome();
    gfx_Begin();
    newline();
    gfx_SetTextXY(2, LCD_HEIGHT - 10);
    printf("lwIP public beta 1\n");
    printf("Simple IRC connect\n");
    lwip_init();
#ifdef ENABLE_VETH
    netif_add(&veth_netif, &addr, &netmask, &gateway, NULL, vethif_init, netif_input);
#endif
    if (usb_Init(cs_handle_usb_event, NULL, NULL /* descriptors */, USB_DEFAULT_INIT_FLAGS))
        return 1;

    // wait for ecm device to be ready
    bool tcp_connected = false;
    uint8_t input_mode = INPUT_LOWER;
    uint8_t string_length = 0;
#define MAX_CHAT_LEN 256
    char chat_string[MAX_CHAT_LEN] = {0};
    bool string_changed = true;
    kb_SetMode(MODE_3_CONTINUOUS);
    do
    {
        if (dhcp_supplied_address(&eth_netif) && !tcp_connected)
        {
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
            gfx_FillScreen(255);
        }
        if (kb_IsDown(kb_KeyAlpha))
        {
            input_mode++;
            if (input_mode > INPUT_NUMBER)
                input_mode = INPUT_LOWER;
            while (kb_IsDown(kb_KeyAlpha))
                ;
        }
        if (kb_IsDown(kb_KeyDel))
        {
            if (string_length > 0)
            {
                string_length--;
                chat_string[string_length] = 0;
                string_changed = true;
            }
            while (kb_IsDown(kb_KeyDel))
                ;
        }
        if (kb_IsDown(kb_KeyEnter))
        {
            if ((string_length > 0) && (tcp_sndbuf(pcb) >= string_length))
            {
                if (tcp_write(pcb, chat_string, string_length, TCP_WRITE_FLAG_COPY) == ERR_OK)
                {
                    tcp_output(pcb);
                    memset(chat_string, 0, string_length);
                    string_length = 0;
                    string_changed = true;
                }
            }
            while (kb_IsDown(kb_KeyEnter))
                ;
        }
        for (size_t i = 0; i < keyconfig[input_mode].len; i++)
        {
            if (kb_IsDown(*(((kb_lkey_t *)keyconfig[input_mode].active_keys) + i)))
            {
                chat_string[string_length++] = *(keyconfig[input_mode].active_chars + i);
                string_changed = true;
            }
            while (kb_IsDown(*(((kb_lkey_t *)keyconfig[input_mode].active_keys) + i)))
                ;
        }

        if (kb_IsDown(kb_KeyClear))
            break;
#ifdef ENABLE_VETH
        veth_receive();
#endif
        usb_HandleEvents();
        sys_check_timeouts();
        if (string_changed && tcp_connected &&)
        {
            gfx_PrintStringXY(chat_string, 0, 230);
            string_changed = false;
        }
    } while (1);
    tcp_abort(pcb);
    dhcp_stop(&eth_netif);
    usb_Cleanup();
    gfx_End();
}
