#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <ti/screen.h>
#include <sys/timers.h>
#include <ti/getcsc.h>
#undef NDEBUG
// #include <debug.h>
// #define ENABLE_VETH 1

#include <usbdrvce.h>
#include "include/lwip/init.h"
#include "include/lwip/netif.h"
#include "include/lwip/altcp_tls.h"
#include "include/lwip/altcp_tcp.h"
#include "include/lwip/altcp.h"
#include "include/lwip/udp.h"
#include "include/lwip/timeouts.h"
#include "include/lwip/sys.h"
#include "include/lwip/snmp.h"
#include "include/lwip/pbuf.h"
#include "include/lwip/dhcp.h"
#include "include/lwip/dns.h"

#include "drivers.h"

enum
{
    INPUT_LOWER,
    INPUT_UPPER,
    INPUT_NUMBER
};

enum _protomode
{
    MODE_TCP,
    MODE_UDP
};
char *chars_lower = "\0\0\0\0\0\0\0\0\0\0\"wrmh\0\0?[vqlg\0\0.zupkfc\0 ytojeb\0\0xsnida\0\0\0\0\0\0\0\0";
char *chars_upper = "\0\0\0\0\0\0\0\0\0\0\"WRMH\0\0?[VQLG\0\0:ZUPKFC\0 YTOJEB\0\0XSNIDA\0\0\0\0\0\0\0\0";
char *chars_num = "\0\0\0\0\0\0\0\0\0\0+-*/^\0\0?359)\0\0\0.258(\0\0\0000147,\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0";
char mode_indic[] = {'a', 'A', '1'};
bool outchar_scroll_up = true;
bool loopit;
bool protomode = MODE_TCP;

ip_addr_t remote_ip = IPADDR4_INIT_BYTES(0, 0, 0, 0);
const char *remote_host = "acagliano.ddns.net";

static void newline(void)
{
    if (outchar_scroll_up)
    {
        memmove(gfx_vram, gfx_vram + (LCD_WIDTH * 10), LCD_WIDTH * (LCD_HEIGHT - 30));
        gfx_SetColor(255);
        gfx_FillRectangle_NoClip(0, LCD_HEIGHT - 30, LCD_WIDTH, 10);
        gfx_SetTextXY(2, LCD_HEIGHT - 30);
    }
    else
        gfx_SetTextXY(2, gfx_GetTextY() + 10);
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
        if (gfx_GetTextX() >= LCD_WIDTH - 16)
        {
            newline();
        }
        gfx_PrintChar(c);
    }
}

struct altcp_pcb *tcp_pcb, *spcb;
struct udp_pcb *udp_pcb;
bool tcp_connected = false;
altcp_allocator_t tcp_allocator = {altcp_tcp_alloc, NULL};
struct altcp_tls_config *tls_conf;

err_t tcp_connect_callback(void *arg, struct altcp_pcb *tpcb, err_t err);

void udp_recv_func(void *arg, struct udp_pcb *pcb, struct pbuf *p, const ip_addr_t *addr, u16_t port)
{
    LWIP_UNUSED_ARG(arg);
    LWIP_UNUSED_ARG(pcb);
    LWIP_UNUSED_ARG(addr);
    LWIP_UNUSED_ARG(port);

    // Data received successfully

    printf("%s\n", p->payload);

    pbuf_free(p);
}

void exit_funcs(void)
{
    if (spcb->state != CLOSED)
        altcp_close(spcb);
    usb_Cleanup();
    gfx_End();
}

void dns_lookup_callback(const char *name, const ip_addr_t *ipaddr, void *callback_arg)
{
    if (ipaddr == NULL)
    {
        printf("dns lookup failed\n");
        exit_funcs();
        exit(1);
    }
    printf("dns lookup ok: %s\n", ipaddr_ntoa(ipaddr));
    memcpy(&remote_ip, ipaddr, sizeof(ip_addr_t));
    if (altcp_connect(spcb, &remote_ip, 8881, tcp_connect_callback) != ERR_OK)
    {
        printf("tcp connect err\n");
        exit_funcs();
        exit(1);
    }
}

void tcp_error_callback(void *arg, err_t err)
{
    struct altcp_pcb *pcb = (struct altcp_pcb *)arg;

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
        altcp_arg(pcb, NULL);
        altcp_sent(pcb, NULL);
        altcp_recv(pcb, NULL);
        altcp_err(pcb, NULL);
        altcp_close(pcb);
    }
}

err_t tcp_recv_callback(void *arg, struct altcp_pcb *tpcb, struct pbuf *p, err_t err)
{
    LWIP_UNUSED_ARG(arg);
    if (err == ERR_OK && p != NULL)
    {
        // Data received successfully
        if (protomode == MODE_TCP)
            printf("%s\n", p->payload);

        // Process or handle the received data
        // process_received_data(p);
        altcp_recved(tpcb, p->tot_len);
        pbuf_free(p);
        return ERR_OK;
    }

    else if (err == ERR_OK && p == NULL)
    {
        // The remote side has closed the connection
        printf("tcp: connection closed by remote\n");
        // Optionally, perform cleanup or take appropriate action
        altcp_close(tpcb);
        loopit = false;

        // The remote host has acknowledged the close
        // You can perform any necessary cleanup here
        return ERR_OK;
    }
    else
    {
        // Handle receive error
        printf("tcp: receive error - %s\n", lwip_strerr(err));
        return err;
    }
}
/*
err_t tcp_sent_callback(void *arg, struct tcp_pcb *tpcb, u16_t len)
{
    LWIP_UNUSED_ARG(arg);
    LWIP_UNUSED_ARG(tpcb);
    // Data has been successfully sent

    // Optionally, perform additional actions after successful send

    return ERR_OK;
}
*/
err_t tcp_connect_callback(void *arg, struct altcp_pcb *tpcb, err_t err)
{
    LWIP_UNUSED_ARG(arg);
    if (err == ERR_OK)
    {
        // Connection successfully established
        //  tcp_sent(tpcb, tcp_sent_callback);
        altcp_recv(tpcb, tcp_recv_callback);
        altcp_err(tpcb, tcp_error_callback);
        tcp_connected = true;
        printf("_____begin text chat_____\n");
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

int main(void)
{
    sk_key_t key = 0;
    char *ref_str = chars_lower;
    os_ClrHome();
    gfx_Begin();
    newline();
    gfx_SetTextXY(2, LCD_HEIGHT - 30);
    printf("lwIP private beta test\n");
    printf("Simple TCP Text Chat\n");
    lwip_init();
    if (usb_Init(eth_handle_usb_event, NULL, NULL, USB_DEFAULT_INIT_FLAGS))
        return 1;

    // wait for ecm device to be ready
    uint8_t input_mode = INPUT_LOWER;
    uint8_t string_length = 0;
    // tls_conf = altcp_tls_create_config_client(NULL, 0);
#define MAX_CHAT_LEN 64
    char chat_string[MAX_CHAT_LEN] = {0};
    char username[16] = {0};
    bool string_changed = true;
    char *active_string = username;
    struct netif *active_netif = NULL;
    loopit = true;
    do
    {
        key = os_GetCSC();
        if (active_netif == NULL)
        {
            active_netif = netif_find("en0");
            if (active_netif)
            {
                dhcp_start(active_netif);
                printf("interface up\n");
            }
        }
        else
        {

            if (key == sk_Alpha)
            {
                input_mode++;
                if (input_mode > INPUT_NUMBER)
                    input_mode = INPUT_LOWER;
                ref_str = (input_mode == INPUT_LOWER) ? chars_lower : (input_mode == INPUT_UPPER) ? chars_upper
                                                                                                  : chars_num;
                string_changed = true;
            }
            else if (key == sk_Mode)
            {
                protomode = (protomode == MODE_TCP);
                if (protomode == MODE_TCP)
                    printf("tcp mode enabled\n");
                else if (protomode == MODE_UDP)
                    printf("udp mode enabled\n");
            }
            else if (key == sk_Del)
            {
                if (string_length > 0)
                {
                    active_string[--string_length] = 0;
                    string_changed = true;
                }
            }
            else if (key == sk_Enter)
            {
                if (active_string == username)
                {
                    err_t dns_resp;
                    if (dhcp_supplied_address(active_netif))
                    {
                        tcp_pcb = altcp_new(&tcp_allocator);
                        udp_pcb = udp_new();
                        if (tcp_pcb == NULL)
                            return 2;
                        // spcb = altcp_tls_wrap(tls_conf, pcb);
                        spcb = tcp_pcb;
                        udp_recv(udp_pcb, udp_recv_func, NULL);
                        altcp_arg(spcb, spcb);
                        dns_resp = dns_gethostbyname(remote_host, &remote_ip, dns_lookup_callback, NULL);
                        printf("dns lookup for: %s\n", remote_host);
                        if (dns_resp == ERR_OK)
                        {
                            printf("host in dns cache\n");
                            if (altcp_connect(spcb, &remote_ip, 8881, tcp_connect_callback) != ERR_OK)
                            {
                                printf("tcp connect err\n");
                                break;
                            }
                        }
                        else if (dns_resp != ERR_INPROGRESS)
                        {
                            printf("hostname lookup err");
                            break;
                        }
                        active_string = chat_string;
                        string_length = 0;
                    }
                    string_changed = true;
                }
                else if (string_length > 0)
                {
                    char tbuf[MAX_CHAT_LEN + 20] = {0};
                    sprintf(tbuf, "[%s] %s", username, chat_string);
                    if (protomode == MODE_TCP)
                    {
                        if (altcp_sndbuf(spcb) >= string_length)
                        {
                            if (altcp_write(spcb, tbuf, strlen(tbuf), TCP_WRITE_FLAG_COPY) == ERR_OK)
                            {
                                altcp_output(spcb);
                            }
                        }
                    }
                    else if (protomode == MODE_UDP)
                    {
                        struct pbuf *tpbuf = pbuf_alloc(PBUF_RAW, strlen(tbuf), PBUF_RAM);
                        if (tpbuf)
                        {
                            pbuf_take(tpbuf, tbuf, strlen(tbuf));
                            if (udp_sendto(udp_pcb, tpbuf, &remote_ip, 8881))
                                printf("udp send error\n");
                        }
                        else
                            printf("udp pbuf alloc error\n");
                    }
                    else
                        printf("invalid protocol mode\n");
                    memset(chat_string, 0, string_length);
                    string_length = 0;
                    string_changed = true;
                }
            }
        }
        if (key == sk_Clear)
        {
            altcp_close(spcb);
            break;
        }
        if (ref_str[key] && (string_length < MAX_CHAT_LEN))
        {
            active_string[string_length++] = ref_str[key];
            string_changed = true;
        }
        if (string_changed)
        {
            outchar_scroll_up = false;
            gfx_SetColor(255);
            gfx_FillRectangle(0, 220, 320, 20);
            gfx_SetColor(0);
            gfx_HorizLine(0, 220, 320);
            gfx_SetTextXY(0, 221);
            if (active_string == username)
                printf("user: ");
            printf("%s", active_string);
            gfx_SetTextFGColor(11);
            gfx_PrintChar(mode_indic[input_mode]);
            gfx_SetTextFGColor(0);
            gfx_SetTextXY(2, LCD_HEIGHT - 30);
            string_changed = false;
            outchar_scroll_up = true;
        }
        usb_HandleEvents();
        sys_check_timeouts();
    } while (loopit == true);
    exit_funcs();
}
