#include <stdint.h>
#include <string.h>
#undef NDEBUG
#include <debug.h>
#include <keypadc.h>
#include "lwip/init.h"
#include "lwip/netif.h"
#include "lwip/timeouts.h"
#include "lwip/snmp.h"
#include "lwip/dhcp.h"
#include "lwip/apps/httpd.h"
#include "lwip/ethip6.h"
#include "veth.h"

#include <usbdrvce.h>

#define ETHERNET_MTU 1500
#define MAX_DEViCES 10

static u8_t mac_addr[] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xab};
struct netif netif;
// uint8_t default_device = 0;
usb_device_t device = NULL;

bool ecm_start(void)
{
    // static uint8_t devnum = 0;
    lwip_init();
    ip4_addr_t addr = IPADDR4_INIT_BYTES(192, 168, 25, 2);
    ip4_addr_t netmask = IPADDR4_INIT_BYTES(255, 255, 255, 0);
    ip4_addr_t gateway = IPADDR4_INIT_BYTES(192, 168, 25, 1);
    netif_add(&netif, &addr, &netmask, &gateway, NULL, my_netif_init, netif_input);
    netif.name[0] = 'e';
    netif.name[1] = 'n';
    netif.num = devnum;
    netif_create_ip6_linklocal_address(&netif, 1);
    netif.ip6_autoconfig_enabled = 1;
    netif_set_status_callback(&netif, netif_status_callback);
    netif_set_default(&netif);
    netif_set_up(&netif);
    // devnum++;
}

usb_error_t ecm_handle_usb_event(usb_event_t event, void *event_data,
                                 usb_callback_data_t *callback_data)
{

    usb_error_t err;

    /* Enable newly connected devices */
    switch (event)
    {
    case USB_DEVICE_CONNECTED_EVENT:
        if (!(usb_GetRole() & USB_ROLE_DEVICE))
        {
            usb_device_t usb_device = event_data;
            usb_ResetDevice(usb_device);
        }
        break;
    case USB_HOST_CONFIGURE_EVENT:
    {
        usb_device_t host = usb_FindDevice(NULL, NULL, USB_SKIP_HUBS);
        if (host)
            usb_device = host;
        if (ecm_start())
            return USB_SUCCESS;
        break;
    }
        // break;
    case USB_DEVICE_RESUMED_EVENT:
    case USB_DEVICE_ENABLED_EVENT:
        usb_device = event_data;
        if (ecm_start())
            return USB_SUCCESS;
        break;
        // break;
    case USB_DEVICE_SUSPENDED_EVENT:
    case USB_DEVICE_DISABLED_EVENT:
    case USB_DEVICE_DISCONNECTED_EVENT:
        srl_Close(&srl);
        RESET_FLAG(gamestate.inet.flags, INET_ACTIVE);
        MARK_FRAME_DIRTY;
        return USB_SUCCESS;
    }
    return USB_SUCCESS;
}

static void
netif_status_callback(struct netif *netif)
{
    dbg_printf("netif status changed %s\n", ip4addr_ntoa(netif_ip4_addr(netif)));
}

static err_t
my_netif_init(struct netif *netif)
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

int main()
{
    if ((error = usb_Init(ecm_handle_usb_event, device, /* ECM descriptors */, USB_DEFAULT_INIT_FLAGS)))
        return false;

    usb_HandleEvents(); // loop this should handle connects?

    /* Start DHCP and HTTPD */
    //    dhcp_start(&netif);
    httpd_init();

    // todo: actually check for this
    netif_set_link_up(&netif);

    kb_SetMode(MODE_3_CONTINUOUS);

    while (!kb_IsDown(kb_KeyClear))
    {
        /* Check link state, e.g. via MDIO communication with PHY */
        //        if(link_state_changed()) {
        //            if(link_is_up()) {
        //                netif_set_link_up(&netif);
        //            } else {
        //                netif_set_link_down(&netif);
        //            }
        //        }
        /* Check for received frames, feed them to lwIP */
        struct pbuf *p = veth_receive();
        if (p != NULL)
        {
            LINK_STATS_INC(link.recv);

            /* Update SNMP stats (only if you use SNMP) */
            //            MIB2_STATS_NETIF_ADD(netif, ifinoctets, p->tot_len);
            //            int unicast = ((p->payload[0] & 0x01) == 0);
            //            if (unicast) {
            //                MIB2_STATS_NETIF_INC(netif, ifinucastpkts);
            //            } else {
            //                MIB2_STATS_NETIF_INC(netif, ifinnucastpkts);
            //            }
            if (netif.input(p, &netif) != ERR_OK)
            {
                pbuf_free(p);
            }
        }

        /* Cyclic lwIP timers check */
        sys_check_timeouts();

        /* your application goes here */
    }
}
