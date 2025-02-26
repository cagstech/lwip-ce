/****************************************************************************
 * Code for Communications Data Class (CDC)
 * for USB-Ethernet devices
 * Includes Callbacks, USB Event handlers, and netif initialization
 */

#include <sys/util.h>
#include <usbdrvce.h>

#if LWIP_USE_CONSOLE_STYLE_PRINTF == LWIP_DBG_ON
#include <graphx.h>
#endif
#if LWIP_DEBUG_LOGFILE == LWIP_DBG_ON
#include <fileioc.h>
#include "lwip/timeouts.h"
#endif

/**
 * LWIP headers for handing link layer to stack,
 * managing NETIF in USB callbacks,
 * managing packet buffers,
 * and tracking link stats
 */
#include "lwip/opt.h"
#include "lwip/debug.h"
#include "lwip/netif.h"
#include "lwip/ethip6.h"
#include "lwip/etharp.h"
#include "lwip/stats.h"
#include "lwip/snmp.h"
#include "lwip/pbuf.h"
#include "lwip/dhcp.h"
#include "lwip/mem.h"
#include <usbdrvce.h>
#include <ethdrvce.h>

/// Define Default Hostname for NETIFs
const char hostname[] = "ti84plusce";
eth_error_t (*lwip_send_fn)(struct eth_device *eth, struct eth_transfer_data *xfer);
eth_error_t (*lwip_open_dev_fn)(struct eth_device *eth,
                     usb_device_t usb_device,
                     eth_transfer_callback_t handle_rx,
                     eth_transfer_callback_t handle_tx,
                     eth_transfer_callback_t handle_int,
                     eth_error_callback_t handle_error
                     );
eth_error_t (*lwip_close_dev_fn)(struct eth_device *eth);

eth_error_t lwip_int_rx_callback(struct eth_device *eth, struct eth_transfer_data *xfer, eth_error_t error){
    struct netif *netif = (struct netif *)eth->user_data;
    if(netif==NULL) {
        LWIP_DEBUGF(ETH_DEBUG | LWIP_DBG_LEVEL_SEVERE,
                    ("INFO: INT no netif"));
        return ETH_OK;
    }
    usb_control_setup_t *notify = (usb_control_setup_t *)xfer->buffer;
    if (notify->bmRequestType == 0b10100001)
    {
        switch (notify->bRequest)
        {
            case NOTIFY_NETWORK_CONNECTION:
                if (notify->wValue && (!netif_is_link_up(netif))){
                    netif_set_link_up(netif);
                    LWIP_DEBUGF(ETH_DEBUG | LWIP_DBG_STATE,
                                ("INFO: netif=%c%c%u, link up", netif->name[0], netif->name[1], netif->num));
                    dhcp_start(netif);
                }
                else if((!notify->wValue) && netif_is_link_up(netif)){
                    netif_set_link_down(netif);
                    LWIP_DEBUGF(ETH_DEBUG | LWIP_DBG_STATE,
                                ("INFO: netif=%c%c%u, link down", netif->name[0], netif->name[1], netif->num));
                }
                break;
            case NOTIFY_CONNECTION_SPEED_CHANGE:
                // this will have no effect - calc too slow
                break;
        }
    }
}

eth_error_t lwip_eth_rx_callback(struct eth_device *eth, struct eth_transfer_data *xfer, eth_error_t error){
    // Ethernet RX callback function. Allocates a buffer for the datagram, passes to netif->input
    struct netif *netif = (struct netif *)eth->user_data;
    if(!netif_is_up(netif)) return ERR_IF;
    if(netif==NULL) {
        LWIP_DEBUGF(ETH_DEBUG | LWIP_DBG_LEVEL_SEVERE,
                    ("INFO: RX no netif"));
        return ETH_OK;
    }
    struct pbuf *p = pbuf_alloc(PBUF_RAW, xfer->len, PBUF_POOL);
    if (p == NULL){
        LWIP_DEBUGF(ETH_DEBUG | LWIP_DBG_LEVEL_SEVERE,
                    ("INFO: RX pbuf alloc failed"));
        return ETH_OK;
    }
    LWIP_DEBUGF(ETH_DEBUG | LWIP_DBG_LEVEL_SEVERE,
                ("INFO: RX len %u", xfer->len));
    pbuf_take(p, xfer->buffer, xfer->len);
    LINK_STATS_INC(link.recv);
    MIB2_STATS_NETIF_ADD(netif, ifinoctets, xfer->len);
    if(netif->input(p, netif) != ERR_OK){
        LWIP_DEBUGF(ETH_DEBUG | LWIP_DBG_LEVEL_SEVERE,
                    ("INFO: RX error"));
        pbuf_free(p);
    }
    return ETH_OK;
}

eth_error_t lwip_eth_tx_callback(struct eth_device *eth, struct eth_transfer_data *xfer, eth_error_t error){
    MEM_CUSTOM_FREE(xfer->buffer);
    MEM_CUSTOM_FREE(xfer);
    return ETH_OK;
}

err_t lwip_tx_output(struct netif *netif, struct pbuf *p){
    LWIP_DEBUGF(ETH_DEBUG | LWIP_DBG_LEVEL_SEVERE,
                ("INFO: TX len %u", p->tot_len));
    struct eth_device* eth = netif->state;
    if(eth==NULL) return ERR_IF;
    struct eth_transfer_data *xfer = MEM_CUSTOM_MALLOC(sizeof(struct eth_transfer_data));
    if(xfer == NULL) {
        LWIP_DEBUGF(ETH_DEBUG | LWIP_DBG_LEVEL_SEVERE,
                    ("INFO: xfer metadata alloc failed"));
        return ERR_MEM;
    }
    memset(xfer, 0, sizeof(struct eth_transfer_data));
    xfer->buffer = MEM_CUSTOM_MALLOC(p->tot_len);
    if(xfer->buffer == NULL) {
        MEM_CUSTOM_FREE(xfer);
        LWIP_DEBUGF(ETH_DEBUG | LWIP_DBG_LEVEL_SEVERE,
                    ("INFO: xfer data block alloc failed"));
        return ERR_MEM;
    }
    pbuf_copy_partial(p, xfer->buffer, p->tot_len, 0);
    LINK_STATS_INC(link.xmit);
    MIB2_STATS_NETIF_ADD(netif, ifoutoctets, p->tot_len);
    if(lwip_send_fn(eth, xfer) != ETH_OK){
        MEM_CUSTOM_FREE(xfer->buffer);
        MEM_CUSTOM_FREE(xfer);
        LWIP_DEBUGF(ETH_DEBUG | LWIP_DBG_LEVEL_SEVERE,
                    ("INFO: TX send fn failure"));
        return ERR_IF;
    }
    return ERR_OK;
}

err_t eth_netif_init(struct netif *netif)
{
    struct eth_device *eth = (struct eth_device *)netif->state;
    if(eth == NULL) LWIP_DEBUGF(ETH_DEBUG | LWIP_DBG_LEVEL_SEVERE,
                                ("INFO: No eth device set"));
    netif->linkoutput = lwip_tx_output;
    netif->output = etharp_output;
    netif->output_ip6 = ethip6_output;
    netif->mtu = ETHERNET_MTU;
    netif->mtu6 = ETHERNET_MTU;
    netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_ETHERNET | NETIF_FLAG_IGMP;
    netif->flags &= ~NETIF_FLAG_MLD6;
    MIB2_INIT_NETIF(netif, snmp_ifType_ethernet_csmacd, 100000000);
    memcpy(netif->hwaddr, eth->hwaddr, NETIF_MAX_HWADDR_LEN);
    netif->hwaddr_len = NETIF_MAX_HWADDR_LEN;
    return ERR_OK;
}

usb_error_t
eth_usb_event_callback(usb_event_t event, void *event_data,
                     __attribute__((unused)) usb_callback_data_t *callback_data)
{
    usb_device_t usb_device = event_data;
    /* Enable newly connected devices */
    switch (event)
    {
        case USB_DEVICE_CONNECTED_EVENT:
            if (!(usb_GetRole() & USB_ROLE_DEVICE)){
                usb_RefDevice(usb_device);
                usb_ResetDevice(usb_device);
            }
            break;
        case USB_DEVICE_ENABLED_EVENT:
        {
            if(usb_GetDeviceFlags(usb_device) & USB_IS_HUB){
                // add handling for hubs
                union
                {
                    uint8_t bytes[256];   // allocate 256 bytes for descriptors
                    usb_configuration_descriptor_t conf; // .. config descriptor alias
                } descriptor;
                size_t desc_len = usb_GetConfigurationDescriptorTotalLength(usb_device, 0);
                size_t xferd;
                usb_GetConfigurationDescriptor(usb_device, 0, &descriptor.conf, desc_len, &xferd);
                if(desc_len != xferd) break;
                usb_SetConfiguration(usb_device, &descriptor.conf, desc_len);
                LWIP_DEBUGF(ETH_DEBUG | LWIP_DBG_STATE,
                            ("INFO: hub=%p", usb_device));
                break;
            }
            struct eth_device *eth = MEM_CUSTOM_MALLOC(sizeof(struct eth_device));
            if(eth == NULL) {
                LWIP_DEBUGF(ETH_DEBUG | LWIP_DBG_LEVEL_SEVERE,
                            ("INFO: eth_device alloc failed dev=%p", usb_device));
                return USB_ERROR_NO_MEMORY;
            }
            if(eth_open(eth, usb_device,
                        lwip_eth_rx_callback,
                        lwip_eth_tx_callback,
                        lwip_int_rx_callback,
                        NULL        // error callback
                        ) != ETH_OK){
                LWIP_DEBUGF(ETH_DEBUG | LWIP_DBG_LEVEL_SEVERE,
                            ("INFO: eth_open failed dev=%p", usb_device));
                MEM_CUSTOM_FREE(eth);
                return USB_ERROR_NO_MEMORY;
            }
            LWIP_DEBUGF(ETH_DEBUG | LWIP_DBG_STATE,
                        ("INFO: ETH dev=%p, mac=%02X:%02X:%02X:%02X:%02X:%02X", usb_device, eth->hwaddr[0], eth->hwaddr[1], eth->hwaddr[2], eth->hwaddr[3], eth->hwaddr[4], eth->hwaddr[5]));
            struct netif *netif = MEM_CUSTOM_MALLOC(sizeof(struct netif));
            if(netif == NULL){
                LWIP_DEBUGF(ETH_DEBUG | LWIP_DBG_LEVEL_SEVERE,
                            ("INFO: netif alloc failed dev=%p", usb_device));
                return USB_ERROR_NO_MEMORY;
            }
            eth->user_data = netif;
            ip4_addr_t ipaddr, netmask, gw;
            ipaddr.addr = IPADDR_ANY;  // Use lwIP’s predefined unassigned address
            netmask.addr = IPADDR_ANY; // Prevent subnet conflicts before DHCP
            gw.addr = IPADDR_ANY;      // No gateway before DHCP

            if (netif_add(netif, &ipaddr, &netmask, &gw, eth, eth_netif_init, netif_input) == NULL)
            {
                LWIP_DEBUGF(ETH_DEBUG | LWIP_DBG_LEVEL_SERIOUS,
                            ("ERROR: netif= <- device=%p, netif add failed", usb_device));
                eth_close(eth);
                MEM_CUSTOM_FREE(eth);
                MEM_CUSTOM_FREE(netif);
                return USB_USER_ERROR;
            }
            netif->name[0] = 'e';
            netif->name[1] = 'n';
            LWIP_DEBUGF(ETH_DEBUG | LWIP_DBG_STATE,
                        ("INFO: ETH link-to NETIF ON %c%c%u", netif->name[0], netif->name[1], netif->num));
            netif_set_default(netif);
            netif_set_hostname(netif, hostname);
            
            // allow IPv4 and IPv6 on device
            netif_create_ip6_linklocal_address(netif, 1);
            netif->ip6_autoconfig_enabled = 1;
            
            netif_set_up(netif);
            LWIP_DEBUGF(ETH_DEBUG | LWIP_DBG_STATE,
                ("INFO: netif=%c%c%u", netif->name[0], netif->name[1], netif->num));
            break;
        }
            
        case USB_DEVICE_DISCONNECTED_EVENT:
        case USB_DEVICE_DISABLED_EVENT:
        {
            struct eth_device *eth = (struct eth_device *)usb_GetDeviceData(usb_device);
            if(eth && eth->user_data){
                struct netif *netif = (struct netif *)eth->user_data;
                    netif_set_link_down(netif);
                    netif_set_down(netif);
                eth_close(eth);
                MEM_CUSTOM_FREE(eth);
                MEM_CUSTOM_FREE(netif);
            }
            LWIP_DEBUGF(ETH_DEBUG | LWIP_DBG_LEVEL_SEVERE,
                        ("INFO: device ptr=%p: disconnected", usb_device));
        }
            if(event == USB_DEVICE_DISCONNECTED_EVENT)
                usb_UnrefDevice(usb_device);
            break;
        case USB_HOST_PORT_CONNECT_STATUS_CHANGE_INTERRUPT:
            LWIP_DEBUGF(ETH_DEBUG | LWIP_DBG_LEVEL_WARNING,
                        ("WARNING: Host port status change event received, ignoring."));
            return USB_ERROR_NO_DEVICE;
            break;
        default:
            break;
    }
    return USB_SUCCESS;
}
