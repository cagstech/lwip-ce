#if ETH_DEBUG_FILE == LWIP_DBG_ON
#include "lwip/timeouts.h"
#endif

#include <usbdrvce.h>
#include <ethdrvce.h>
#include <ti/getcsc.h>
#include <ti/screen.h>

#include "lwip/init.h"
#include "lwip/timeouts.h"
#include "lwip/netif.h"
#include "lwip/mem.h"

/* These commented out, but may be headers you might wish to enable. */
// #include "lwip/altcp_tcp.h"
// #include "lwip/altcp.h"
// #include "lwip/udp.h"
#include "lwip/dhcp.h"
// #include "lwip/dns.h"
#include "lwip/apps/httpd.h"
// due to the build structure of lwIP, "lwip/file.h" corresponds to "include/lwip/file.h"

#include "drivers/usb_ethernet.h"

bool run_main = false;
bool dhcp_started = false;
bool httpd_running = false;

#define LWIP_USE_CONSOLE_STYLE_PRINTF
#ifdef LWIP_USE_CONSOLE_STYLE_PRINTF
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

void __attribute__((optnone)) outchar(char c)
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
#endif


int main(void)
{
    
    os_ClrLCDFull();
    os_HomeUp();
    os_FontSelect(os_SmallFont);
    gfx_Begin();
    gfx_FillScreen(255);
    uint8_t key;
#define LWIP_CONFIG_V1  0
    struct lwip_configurator lwip_cfg = {
        LWIP_CONFIG_V1,
        eth_send,
        eth_open,
        eth_close,
        malloc,
        free
    };
    netif_default = NULL;
    
    if(lwip_init(&lwip_cfg)!= ERR_OK)
        return 1;

    /* You should probably handle this function failing */
    if (usb_Init(eth_usb_event_callback, NULL, NULL, USB_DEFAULT_INIT_FLAGS))
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
        if (netif_default)
        {
            // run this code if netif exists
            // eg: dhcp_start(ethif);
            if (dhcp_supplied_address(netif_default) && (!httpd_running))
            {
                httpd_init();
                printf("httpd listen on %s\n", ip4addr_ntoa(netif_ip4_addr(netif_default)));
                httpd_running = true;
            }
        }
        usb_HandleEvents();   // usb events
        sys_check_timeouts(); // lwIP timers/event callbacks
    } while (run_main);
    dhcp_release_and_stop(netif_default);
exit:
    usb_Cleanup();
    gfx_End();
    exit(0);
}
