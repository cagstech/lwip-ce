#include <stdint.h>
#include <string.h>
#include <stdio.h>
#undef NDEBUG
// #include <debug.h>
#include <keypadc.h>
#include <usbdrvce.h>
// #include "lwip/init.h"
// #include "lwip/netif.h"
// #include "lwip/timeouts.h"
// #include "lwip/snmp.h"
// #include "lwip/dhcp.h"
// #include "lwip/apps/httpd.h"
// #include "lwip/ethip6.h"
#include "ecm.h"

// #define ETHERNET_MTU 1500
// #define MAX_DEViCES 10

// static u8_t mac_addr[] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xab};
// struct netif netif;
//  uint8_t default_device = 0;

/*
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
*/
int main(void)
{
    // lwip_init();
    if (usb_Init(ecm_handle_usb_event, ecm_device.usb_device, NULL /* descriptors */, USB_DEFAULT_INIT_FLAGS))
        return 1;

    kb_SetMode(MODE_3_CONTINUOUS);
    bool init_done = false;
    do
    {
        kb_Scan();
        if (init_done == false && ecm_device.ready)
        {
            printf("Device ready!\n");
            ecm_receive();
            init_done = true;
        }
        if (kb_IsDown(kb_2nd))
        {
            const char *msg1 = "The fox jumped over the dog.";
            ecm_transmit(msg1, strlen(msg1));
        }
        if (kb_IsDown(kb_Alpha))
        {
            const char *msg2 = "The fox jumped over the other fox.";
            ecm_transmit(msg2, strlen(msg2));
        }
        usb_HandleEvents();
        if (kb_IsDown(kb_Clear))
            break;
    } while (1);

    usb_Cleanup();
    /* Start DHCP and HTTPD */
    //    dhcp_start(&netif);
    // httpd_init();

    // todo: actually check for this
    // netif_set_link_up(&netif);

    // kb_SetMode(MODE_3_CONTINUOUS);
}
