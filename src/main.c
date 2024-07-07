
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
bool dhcp_started = false;
bool httpd_running = false;
bool outchar_scroll_up = true;

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
    gfx_Begin();
    gfx_FillScreen(255);
    lwip_init();
    struct netif *ethif = NULL;

    /* You should probably handle this function failing */
    if (usb_Init(eth_handle_usb_event, NULL, NULL, USB_DEFAULT_INIT_FLAGS))
        goto exit;

    run_main = true;

    do
    {
        // this is your code that runs in a loop
        // please note that much of the networking in lwIP is callback-style
        // please consult the lwIP documentation for the protocol you are using for instructions
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
                netif_set_default(ethif);
                netif_set_status_callback(ethif, ethif_status_callback_fn);
                dhcp_start(ethif);
            }
        }
        usb_HandleEvents();   // usb events
        sys_check_timeouts(); // lwIP timers/event callbacks
    } while (run_main);
    dhcp_release_and_stop(ethif);
exit:
    usb_Cleanup();
    gfx_End();
    exit(0);
}
