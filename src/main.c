#include <stdint.h>
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
#include "include/lwip/timeouts.h"
#include "include/lwip/snmp.h"
#include "include/lwip/dhcp.h"
#include "include/lwip/apps/httpd.h"
#include "include/lwip/ethip6.h"
#include "ecm.h"

#define MAX_DEViCES 10

static void
netif_status_callback(struct netif *netif)
{
    dbg_printf("netif status changed %s\n", ip4addr_ntoa(netif_ip4_addr(netif)));
}

static err_t
my_netif_init(struct netif *netif)
{
    netif->linkoutput = ecm_transmit;
    netif->output = etharp_output;
    netif->output_ip6 = ethip6_output;
    netif->mtu = ECM_MTU;
    netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_ETHERNET | NETIF_FLAG_IGMP | NETIF_FLAG_MLD6;
    MIB2_INIT_NETIF(netif, snmp_ifType_ethernet_csmacd, 100000000);
    memcpy(netif->hwaddr, ecm_device.usb.mac_addr, ETH_HWADDR_LEN);
    netif->hwaddr_len = ETH_HWADDR_LEN;
    return ERR_OK;
}

int main(void)
{
    os_ClrHomeFull();
    // initialize usb hardware
    if (usb_Init(ecm_handle_usb_event, NULL, NULL /* descriptors */, USB_DEFAULT_INIT_FLAGS))
        return 1;

    // wait for ecm device to be ready
    bool netif_init = false;
    kb_SetMode(MODE_3_CONTINUOUS);
    do
    {
        if (ecm_device.status == ECM_READY && netif_init == false)
        {
            // queue ecm_receive
            // configure ip addr info
            printf("device ready\n");
            lwip_init();
            ip4_addr_t addr = IPADDR4_INIT_BYTES(192, 168, 25, 2);
            ip4_addr_t netmask = IPADDR4_INIT_BYTES(255, 255, 255, 0);
            ip4_addr_t gateway = IPADDR4_INIT_BYTES(192, 168, 1, 1);
            netif_add(&ecm_device.netif, &addr, &netmask, &gateway, NULL, my_netif_init, netif_input);
            ecm_device.netif.name[0] = 'e';
            ecm_device.netif.name[1] = 'n';
            ecm_device.netif.num = 0;
            netif_create_ip6_linklocal_address(&ecm_device.netif, 1);
            ecm_device.netif.ip6_autoconfig_enabled = 1;
            netif_set_status_callback(&ecm_device.netif, netif_status_callback);
            netif_set_default(&ecm_device.netif);
            netif_set_up(&ecm_device.netif);
            netif_init = true;
        }
        if (kb_IsDown(kb_KeyClear))
            break;
        usb_HandleEvents();
    } while (1);

    usb_Cleanup();
    /* Start DHCP and HTTPD */
    //    dhcp_start(&netif);
    // httpd_init();

    // todo: actually check for this
    // netif_set_link_up(&netif);

    // kb_SetMode(MODE_3_CONTINUOUS);
}
