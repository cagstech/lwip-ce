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
#include "usb_ethernet.h" /* Communications Data Class header file */

#define NETIFS_MAX_ALLOWED 8
#define CHECK_BIT(var, pos) ((var) & (1 << (pos)))

/// Define Default Hostname for NETIFs
const char hostname[] = "ti84plusce";
static uint8_t ifnums_used = 0;

struct eth_configurator eth_conf = {
    ETH_CONFIGURATOR_V1,
    USB_CDC_MAX_RETRIES,
    true,
    true
};


/// UTF-16 -> hex conversion
uint8_t
nibble(uint16_t c)
{
    c -= '0';
    if (c < 10)
        return c;
    c -= 'A' - '0';
    if (c < 6)
        return c + 10;
    c -= 'a' - 'A';
    if (c < 6)
        return c + 10;
    return 0xff;
}

void eth_device_teardown(eth_device_t *dev){
    usb_SetDeviceData(dev->device, NULL);
    ifnums_used &= ~(1 << dev->iface.num);
    netif_remove(&dev->iface);
    free(dev);
}

bool eth_xmit_fatal_error(eth_device_t *dev, uint8_t retries){
    if(retries == eth_conf.max_retries){
        LWIP_DEBUGF(ETH_DEBUG | LWIP_DBG_LEVEL_SEVERE,
                    ("ERROR: device ptr=%p: fatal error", dev->device));
        if(eth_conf.do_reset_on_error){
            LWIP_DEBUGF(ETH_DEBUG | LWIP_DBG_LEVEL_SEVERE,
                        ("INFO: device ptr=%p: resetting", dev->device));
            if(usb_ResetDevice(dev->device)){
                eth_device_teardown(dev);
                LWIP_DEBUGF(ETH_DEBUG | LWIP_DBG_LEVEL_SEVERE,
                            ("ERROR: device ptr=%p: reset fail, teardown", dev->device));
            }
        }
        return true;
    }
    return false;
}

void timeout_change_default_netif(void *arg){
    // if link is stale for 5000ms, disable device, unregister interface
    eth_device_t *dev = (eth_device_t *)arg;
    if(netif_is_link_up(&dev->iface)) return;
    if(netif_default != &dev->iface) return;
    
    // if this is the default interface
    uint8_t ifname[] = "en0";       // try to set a new one
    uint8_t ifnum_check = 0;
    for (ifnum_check = 0; ifnum_check < NETIFS_MAX_ALLOWED; ifnum_check++){
        if(dev->iface.num == ifnum_check) continue;     // skip netif now offline
        if (CHECK_BIT(ifnums_used, ifnum_check)){       // if netif is registered
            ifname[2] = ifnum_check;
            struct netif *new_default = netif_find(ifname);
            if(netif_is_link_up(new_default)) {         // if link is up
                netif_set_default(new_default);         // set new default
                LWIP_DEBUGF(ETH_DEBUG | LWIP_DBG_STATE,
                            ("INFO: setting new default, netif=%c%c%u", new_default->name[0], new_default->name[1], new_default->num));
                return;
            }
        }
    }
    netif_set_default(NULL);
    LWIP_DEBUGF(ETH_DEBUG | LWIP_DBG_STATE,
                ("INFO: setting new default, netif=NULL"));
}


///---------------------------------------------------
/// @brief interrupt transfer callback function
usb_error_t
interrupt_receive_callback(__attribute__((unused)) usb_endpoint_t endpoint,
                           usb_transfer_status_t status,
                           size_t transferred,
                           usb_transfer_data_t *data)
{
    eth_device_t *dev = (eth_device_t *)data;
    uint8_t *ibuf = dev->interrupt.buf;
    static uint8_t int_retries = 0;
    if (status)
    {
        // much like RX, we will retry a INT USB_CDC_MAX_RETRIES times
        if(eth_xmit_fatal_error(dev, int_retries))
            return USB_ERROR_FAILED;
        LWIP_DEBUGF(ETH_DEBUG | LWIP_DBG_LEVEL_SERIOUS,
                    ("ERROR: int endpoint failure, retry=%u", int_retries));
        // increment TX retry counter and queue the transfer again
        int_retries++;
    } else if ((status == USB_TRANSFER_COMPLETED) && transferred)
    {
        usb_control_setup_t *notify;
        size_t bytes_parsed = 0;
        do
        {
            notify = (usb_control_setup_t *)&ibuf[bytes_parsed];
            if (notify->bmRequestType == 0b10100001)
            {
                switch (notify->bRequest)
                {
                    case NOTIFY_NETWORK_CONNECTION:
                        if (notify->wValue && (!netif_is_link_up(&dev->iface))){
                            netif_set_link_up(&dev->iface);
                            LWIP_DEBUGF(ETH_DEBUG | LWIP_DBG_STATE,
                                        ("INFO: netif=%c%c%u, link up", dev->iface.name[0], dev->iface.name[1], dev->iface.num));
                            if(eth_conf.do_dhcp_auto)
                                dhcp_start(&dev->iface);
                            if(!netif_default){
                                LWIP_DEBUGF(ETH_DEBUG | LWIP_DBG_STATE,
                                            ("INFO: setting new default, netif=%c%c%u", dev->iface.name[0], dev->iface.name[1], dev->iface.num));
                                netif_set_default(&dev->iface);
                            }
                        }
                        else if((!notify->wValue) && netif_is_link_up(&dev->iface)){
                            netif_set_link_down(&dev->iface);
                            LWIP_DEBUGF(ETH_DEBUG | LWIP_DBG_STATE,
                                        ("INFO: netif=%c%c%u, link down", dev->iface.name[0], dev->iface.name[1], dev->iface.num));
                            sys_timeout(5000, timeout_change_default_netif, dev);
                        }
                        break;
                    case NOTIFY_CONNECTION_SPEED_CHANGE:
                        // this will have no effect - calc too slow
                        break;
                }
            }
            bytes_parsed += sizeof(usb_control_setup_t) + notify->wLength;
        } while (bytes_parsed < transferred);
        int_retries = 0;
    }
    usb_ScheduleInterruptTransfer(dev->interrupt.endpoint, dev->interrupt.buf, INTERRUPT_RX_MAX, interrupt_receive_callback, data);
    return USB_SUCCESS;
}

///---------------------------------------------------
/// @brief bulk out callback function
usb_error_t bulk_transmit_callback(__attribute__((unused)) usb_endpoint_t endpoint,
                                   usb_transfer_status_t status,
                                   __attribute__((unused)) size_t transferred,
                                   usb_transfer_data_t *data)
{
    // Handle completion or error of the transfer, if needed
    eth_device_t *dev = (eth_device_t *)data;
    static uint8_t tx_retries = 0;
    if (status)
    {
        // much like RX, we will retry a TX USB_CDC_MAX_RETRIES times
        struct pbuf *tbuf = (struct pbuf*)data;
        if(eth_xmit_fatal_error(dev, tx_retries)){
            pbuf_free(data);
            return USB_ERROR_FAILED;
        }
        LWIP_DEBUGF(ETH_DEBUG | LWIP_DBG_LEVEL_SERIOUS,
                    ("INFO: tx endpoint failure, retry=%u", tx_retries));
        // increment TX retry counter and queue the transfer again
        tx_retries++;
        usb_ScheduleBulkTransfer(dev->tx.endpoint, tbuf->payload, tbuf->tot_len, bulk_transmit_callback, tbuf);
        return USB_SUCCESS;
    }
    if (data)
        pbuf_free(data);
    
    return USB_SUCCESS;
}

///------------------------------------------------------------------------
/// @brief linkinput callback function for @b Ethernet_Control_Model (ECM)
usb_error_t ecm_receive_callback(__attribute__((unused)) usb_endpoint_t endpoint,
                                 usb_transfer_status_t status,
                                 size_t transferred,
                                 usb_transfer_data_t *data)
{
    eth_device_t *dev = (eth_device_t *)data;
    uint8_t *recvbuf = dev->rx.buf;
    static uint8_t rx_retries = 0;
    if (status)
    {
        if(eth_xmit_fatal_error(dev, rx_retries))
            return USB_ERROR_FAILED;
        
        LWIP_DEBUGF(ETH_DEBUG | LWIP_DBG_LEVEL_SERIOUS,
                    ("INFO: rx endpoint failure, retry=%u", rx_retries));
        rx_retries++;
        usb_ScheduleBulkTransfer(dev->rx.endpoint, dev->rx.buf, ETHERNET_MTU, dev->rx.callback, data);
        return USB_SUCCESS;
    } else if (transferred)
    {
        rx_retries = 0;
        struct pbuf *p = pbuf_alloc(PBUF_RAW, transferred, PBUF_POOL);
        if (p == NULL)
            return USB_ERROR_NO_MEMORY;
        pbuf_take(p, recvbuf, transferred);
        LINK_STATS_INC(link.recv);
        MIB2_STATS_NETIF_ADD(&dev->iface, ifinoctets, transferred);
        
        usb_ScheduleBulkTransfer(dev->rx.endpoint, dev->rx.buf, ETHERNET_MTU, dev->rx.callback, data);
        
        if (dev->iface.input(p, &dev->iface) != ERR_OK)
            pbuf_free(p);
    }
    return USB_SUCCESS;
}

///----------------------------------------------------------------
/// @brief linkoutput function for @b Ethernet_Control_Model (ECM)
err_t ecm_bulk_transmit(struct netif *netif, struct pbuf *p)
{
    eth_device_t *dev = (eth_device_t *)netif->state;
    if (p->tot_len > ETHERNET_MTU)
        return ERR_MEM;
    struct pbuf *tbuf = pbuf_alloc(PBUF_RAW, p->tot_len, PBUF_RAM);
    LINK_STATS_INC(link.xmit);
    // Update SNMP stats(only if you use SNMP)
    MIB2_STATS_NETIF_ADD(netif, ifoutoctets, p->tot_len);
    if (pbuf_copy(tbuf, p))
        return ERR_MEM;
    usb_ScheduleBulkTransfer(dev->tx.endpoint, tbuf->payload, tbuf->tot_len, bulk_transmit_callback, tbuf);
    return ERR_OK;
}

/****************************************************************************
 * Code for Network Control Model (NCM)
 * for USB-Ethernet devices
 */

/* Flag Values for NCM Network Capabilities. */
enum _cdc_ncm_bm_networkcapabilities
{
    CAPABLE_ETHERNET_PACKET_FILTER,
    CAPABLE_NET_ADDRESS,
    CAPABLE_ENCAPSULATED_RESPONSE,
    CAPABLE_MAX_DATAGRAM,
    CAPABLE_NTB_INPUT_SIZE_8BYTE
};
/* Helper Macro for Returning State of Network Capabilities Flag. */
#define ncm_device_supports(dev, bm) (((dev)->class.ncm.bm_capabilities >> (bm)) & 1)

/* Data Structure for NTB Config control request. */
struct _ntb_config_data
{
    uint32_t dwNtbInMaxSize;
    uint16_t wNtbInMaxDatagrams;
    uint16_t reserved;
};

/* NCM Transfer Header (NTH) Defintion */
#define NCM_NTH_SIG 0x484D434E
struct ncm_nth
{
    uint32_t dwSignature;   // "NCMH"
    uint16_t wHeaderLength; // size of this header structure (should be 12 for NTB-16)
    uint16_t wSequence;     // counter for NTB's sent
    uint16_t wBlockLength;  // size of the NTB
    uint16_t wNdpIndex;     // offset to first NDP
};

/* NCM Datagram Pointers (NDP) Definition */
struct ncm_ndp_idx
{
    uint16_t wDatagramIndex; // offset of datagram, if 0, then is end of datagram list
    uint16_t wDatagramLen;   // length of datagram, if 0, then is end of datagram list
};
#define NCM_NDP_SIG0 0x304D434E
#define NCM_NDP_SIG1 0x314D434E
struct ncm_ndp
{
    uint32_t dwSignature;               // "NCM0"
    uint16_t wLength;                   // size of NDP16
    uint16_t wNextNdpIndex;             // offset to next NDP16
    struct ncm_ndp_idx wDatagramIdx[1]; // pointer to end of NDP
};

#define NCM_NTH_LEN sizeof(struct ncm_nth)
#define NCM_NDP_LEN sizeof(struct ncm_ndp)
#define NCM_RX_MAX_DATAGRAMS 4
#define NCM_RX_DATAGRAMS_OVERFLOW_MUL 16 // this is here in the event that max datagrams is unsupported
#define NCM_RX_QUEUE_LEN (NCM_RX_MAX_DATAGRAMS * NCM_RX_DATAGRAMS_OVERFLOW_MUL)

///------------------------------------------------------------
/// @brief control setup for @b Network_Control_Model (NCM)
usb_error_t ncm_control_setup(eth_device_t *eth)
{
    if (eth->type != USB_NCM_SUBCLASS)
        return USB_SUCCESS;
    size_t transferred;
    usb_error_t error = 0;
    usb_control_setup_t get_ntb_params = {0b10100001, REQUEST_GET_NTB_PARAMETERS, 0, 0, 0x1c};
    usb_control_setup_t ntb_config_request = {0b00100001, REQUEST_SET_NTB_INPUT_SIZE, 0, 0, ncm_device_supports(eth, CAPABLE_NTB_INPUT_SIZE_8BYTE) ? 8 : 4};
    // usb_control_setup_t multicast_filter_request = {0b00100001, REQUEST_SET_ETHERNET_MULTICAST_FILTERS, 0, 0, 0};
    usb_control_setup_t packet_filter_request = {0b00100001, REQUEST_SET_ETHERNET_PACKET_FILTER, 0x01, 0, 0};
    struct _ntb_config_data ntb_config_data = {NCM_RX_NTB_MAX_SIZE, NCM_RX_MAX_DATAGRAMS, 0};
    
    /* Query NTB Parameters for device (NCM devices) */
    error |= usb_DefaultControlTransfer(eth->device, &get_ntb_params, &eth->class.ncm.ntb_params, USB_CDC_MAX_RETRIES, &transferred);
    
    /* Set NTB Max Input Size to 2048 (recd minimum NCM spec v 1.2) */
    error |= usb_DefaultControlTransfer(eth->device, &ntb_config_request, &ntb_config_data, USB_CDC_MAX_RETRIES, &transferred);
    
    /* Reset packet filters */
    if (ncm_device_supports(eth, CAPABLE_ETHERNET_PACKET_FILTER))
        error |= usb_DefaultControlTransfer(eth->device, &packet_filter_request, NULL, USB_CDC_MAX_RETRIES, &transferred);
    
    return error;
}

///------------------------------------------------------------
/// @brief linkinput function for @b Network_Control_Model (NCM)
usb_error_t ncm_receive_callback(__attribute__((unused)) usb_endpoint_t endpoint,
                                 usb_transfer_status_t status,
                                 size_t transferred,
                                 usb_transfer_data_t *data)
{
    eth_device_t *dev = (eth_device_t *)data;
    uint8_t *recvbuf = dev->rx.buf;
    static uint8_t rx_retries = 0;
    if (status)
    {
        if(eth_xmit_fatal_error(dev, rx_retries)){
            pbuf_free(data);
            return USB_ERROR_FAILED;
        }
        LWIP_DEBUGF(ETH_DEBUG | LWIP_DBG_LEVEL_SERIOUS,
                        ("INFO: rx endpoint failure, retry=%u", rx_retries));
        rx_retries++;
        usb_ScheduleBulkTransfer(dev->rx.endpoint, dev->rx.buf, NCM_RX_NTB_MAX_SIZE, dev->rx.callback, data);
        return USB_SUCCESS;
    }
    if (transferred)
    {
        rx_retries = 0;
        struct pbuf *rx_queue[NCM_RX_QUEUE_LEN];
        uint16_t enqueued = 0;
        bool parse_ntb = true;
        
        // get header and first NDP pointers
        uint8_t *ntb = (uint8_t *)recvbuf;
        struct ncm_nth *nth = (struct ncm_nth *)ntb;
        if (nth->dwSignature != NCM_NTH_SIG)
            return USB_SUCCESS; // validate NTH signature field. If invalid, fail out
        
        // start proc'ing first NDP
        struct ncm_ndp *ndp = (struct ncm_ndp *)&ntb[nth->wNdpIndex];
        
        // repeat while ndp->wNextNdpIndex is non-zero
        do
        {
            if (ndp->dwSignature != NCM_NDP_SIG0)
                return USB_SUCCESS; // validate NDP signature field, if invalid, fail out
            
            // set datagram number to 0 and set datagram index pointer
            uint16_t dg_num = 0;
            struct ncm_ndp_idx *idx = (struct ncm_ndp_idx *)&ndp->wDatagramIdx;
            
            // a null datagram index structure indicates end of NDP
            do
            {
                // attempt to allocate pbuf
                if (enqueued >= NCM_RX_QUEUE_LEN)
                {
                    parse_ntb = false;
                    break;
                }
                struct pbuf *p = pbuf_alloc(PBUF_RAW, idx[dg_num].wDatagramLen, PBUF_POOL);
                if (p == NULL)
                { // if allocation failed, break loops
                    parse_ntb = false;
                    break;
                }
                if (pbuf_take(p, &ntb[idx[dg_num].wDatagramIndex], idx[dg_num].wDatagramLen))
                { // if pbuf take fails, break loops
                    parse_ntb = false;
                    break;
                }
                // enqueue packet, we will finish processing first
                rx_queue[enqueued++] = p;
                dg_num++;
            } while ((idx[dg_num].wDatagramIndex) && (idx[dg_num].wDatagramLen));
            // if next NDP is 0, NTB is done and so is my sanity
            if (ndp->wNextNdpIndex == 0)
                break;
            // set next NDP
            ndp = (struct ncm_ndp *)&ntb[ndp->wNextNdpIndex];
        } while (parse_ntb);
        
        LINK_STATS_INC(link.recv);
        MIB2_STATS_NETIF_ADD(&dev->iface, ifinoctets, transferred);
        
        // queue up next transfer first
        usb_ScheduleBulkTransfer(dev->rx.endpoint, dev->rx.buf, NCM_RX_NTB_MAX_SIZE, dev->rx.callback, data);
        
        // hand packet queue to lwIP
        for (int i = 0; i < enqueued; i++)
            if (rx_queue[i])
                if (dev->iface.input(rx_queue[i], &dev->iface) != ERR_OK)
                    pbuf_free(rx_queue[i]);
    }
    return USB_SUCCESS;
}

/* This code packs a single TX Ethernet frame into an NCM transfer. */
#define NCM_HBUF_SIZE 64
#define get_next_offset(offset, divisor, remainder) \
(offset) + (divisor) - ((offset) % (divisor)) + (remainder)

///---------------------------------------------------------------
/// @brief linkoutput function for @b Network_Control_Model (NCM)
err_t ncm_bulk_transmit(struct netif *netif, struct pbuf *p)
{
    eth_device_t *dev = (eth_device_t *)netif->state;
    uint16_t offset_ndp = get_next_offset(NCM_NTH_LEN, dev->class.ncm.ntb_params.wNdpInAlignment, 0);
    if (p->tot_len > ETHERNET_MTU)
        return ERR_MEM;
    
    // allocate TX packet buffer
    struct pbuf *obuf = pbuf_alloc(PBUF_RAW, ETHERNET_MTU + NCM_HBUF_SIZE, PBUF_RAM);
    if (obuf == NULL)
        return ERR_MEM;
    
    memset(obuf->payload, 0, ETHERNET_MTU + NCM_HBUF_SIZE);
    
    // declare NTH, NDP, and NDP_IDX structures
    uint8_t hdr_buf[NCM_HBUF_SIZE] = {0};
    struct ncm_nth *nth = (struct ncm_nth *)hdr_buf;
    struct ncm_ndp *ndp = (struct ncm_ndp *)&hdr_buf[offset_ndp];
    
    // populate structs
    nth->dwSignature = NCM_NTH_SIG;
    nth->wHeaderLength = NCM_NTH_LEN;
    nth->wSequence = dev->class.ncm.sequence++;
    nth->wBlockLength = NCM_HBUF_SIZE + ETHERNET_MTU;
    nth->wNdpIndex = offset_ndp;
    
    ndp->dwSignature = NCM_NDP_SIG0;
    ndp->wLength = NCM_NDP_LEN + 4;
    ndp->wNextNdpIndex = 0;
    
    ndp->wDatagramIdx[0].wDatagramIndex = NCM_HBUF_SIZE;
    ndp->wDatagramIdx[0].wDatagramLen = p->tot_len;
    ndp->wDatagramIdx[1].wDatagramIndex = 0;
    ndp->wDatagramIdx[1].wDatagramLen = 0;
    
    // absorb the populated structs into the obuf
    pbuf_take(obuf, hdr_buf, NCM_HBUF_SIZE);
    
    // copy the datagram to the pbuf
    pbuf_copy_partial(p, obuf->payload + NCM_HBUF_SIZE, p->tot_len, 0);
    LINK_STATS_INC(link.xmit);
    // Update SNMP stats(only if you use SNMP)
    MIB2_STATS_NETIF_ADD(netif, ifoutoctets, p->tot_len);
    
    // queue the TX
    // printf("sent packet %u at time %lu\n", sequence, sys_now());
    usb_ScheduleBulkTransfer(dev->tx.endpoint, obuf->payload, ETHERNET_MTU + NCM_HBUF_SIZE, bulk_transmit_callback, obuf);
    return ERR_OK;
}

///----------------------------------------
/// @brief ethernet NETIF initialization
err_t eth_netif_init(struct netif *netif)
{
    eth_device_t *dev = (eth_device_t *)netif->state;
    netif->linkoutput = dev->tx.emit;
    netif->output = etharp_output;
    netif->output_ip6 = ethip6_output;
    netif->mtu = ETHERNET_MTU;
    netif->mtu6 = ETHERNET_MTU;
    netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_ETHERNET | NETIF_FLAG_IGMP | NETIF_FLAG_MLD6;
    MIB2_INIT_NETIF(netif, snmp_ifType_ethernet_csmacd, 100000000);
    memcpy(netif->hwaddr, dev->hwaddr, NETIF_MAX_HWADDR_LEN);
    netif->hwaddr_len = NETIF_MAX_HWADDR_LEN;
    // netif_set_link_callback(netif, eth_link_callback);
    // netif_set_status_callback(netif, eth_status_callback);
    return ERR_OK;
}

/** DESCRIPTOR PARSER AWAIT STATES */
enum _descriptor_parser_await_states
{
    PARSE_HAS_CONTROL_IF = 1,
    PARSE_HAS_MAC_ADDR = (1 << 1),
    PARSE_HAS_BULK_IF_NUM = (1 << 2),
    PARSE_HAS_BULK_IF = (1 << 3),
    PARSE_HAS_ENDPOINT_INT = (1 << 4),
    PARSE_HAS_ENDPOINT_IN = (1 << 5),
    PARSE_HAS_ENDPOINT_OUT = (1 << 6)
};

/*****************************************************************************************
 * @brief Parses descriptors for a USB device and checks for a valid CDC Ethernet device.
 * @return \b True if success (with NETIF initialized), \b False if not CDC-ECM/NCM or error.
 */
#define DESCRIPTOR_MAX_LEN 256
bool init_ethernet_usb_device(usb_device_t device)
{
    eth_device_t tmp = {0};
    eth_device_t *eth = NULL;
    size_t xferd, parsed_len, desc_len;
    usb_error_t err;
    tmp.device = device;
    
    if(ifnums_used == 0b11111111){
        LWIP_DEBUGF(ETH_DEBUG | LWIP_DBG_LEVEL_WARNING,
                    ("WARNING, device=%p, if_slots full", device));
        return false;
    }
    
    struct
    {
        usb_configuration_descriptor_t *addr;
        size_t len;
    } configdata;
    struct
    {
        usb_interface_descriptor_t *addr;
        size_t len;
    } if_bulk;
    struct
    {
        uint8_t in, out, interrupt;
    } endpoint_addr;
    union
    {
        uint8_t bytes[DESCRIPTOR_MAX_LEN];   // allocate 256 bytes for descriptors
        usb_descriptor_t desc;               // descriptor type aliases
        usb_device_descriptor_t dev;         // .. device descriptor alias
        usb_configuration_descriptor_t conf; // .. config descriptor alias
    } descriptor;
    
    err = usb_GetDeviceDescriptor(device, &descriptor.dev, sizeof(usb_device_descriptor_t), &xferd);
    if (err || (xferd != sizeof(usb_device_descriptor_t)))
        return false;
    
    // check for main DeviceClass being type 0x00 - composite/if-specific
    if (descriptor.dev.bDeviceClass != USB_INTERFACE_SPECIFIC_CLASS)
        return false;

    // parse both configs for the correct one
    uint8_t num_configs = descriptor.dev.bNumConfigurations;
    for (uint8_t config = 0; config < num_configs; config++)
    {
        uint8_t ifnum = 0;
        uint8_t parse_state = 0;
        desc_len = usb_GetConfigurationDescriptorTotalLength(device, config);
        parsed_len = 0;
        if (desc_len > 256)
            // if we overflow buffer, skip descriptor
            continue;
        // fetch config descriptor
        err = usb_GetConfigurationDescriptor(device, config, &descriptor.conf, desc_len, &xferd);
        if (err || (xferd != desc_len))
            // if error or not full descriptor, skip
            continue;
        
        // set pointer to current working descriptor
        usb_descriptor_t *desc = &descriptor.desc;
        while (parsed_len < desc_len)
        {
            switch (desc->bDescriptorType)
            {
                case USB_ENDPOINT_DESCRIPTOR:
                {
                    // we should only look for this IF we have found the ECM control interface,
                    // and have retrieved the bulk data interface number from the CS_INTERFACE descriptor
                    // see case USB_CS_INTERFACE_DESCRIPTOR and case USB_INTERFACE_DESCRIPTOR
                    usb_endpoint_descriptor_t *endpoint = (usb_endpoint_descriptor_t *)desc;
                    if (parse_state & PARSE_HAS_BULK_IF)
                    {
                        if (endpoint->bEndpointAddress & (USB_DEVICE_TO_HOST))
                        {
                            endpoint_addr.in = endpoint->bEndpointAddress; // set out endpoint address
                            parse_state |= PARSE_HAS_ENDPOINT_IN;
                        }
                        else
                        {
                            endpoint_addr.out = endpoint->bEndpointAddress; // set in endpoint address
                            parse_state |= PARSE_HAS_ENDPOINT_OUT;
                        }
                        if ((parse_state & PARSE_HAS_ENDPOINT_IN) &&
                            (parse_state & PARSE_HAS_ENDPOINT_OUT) &&
                            (parse_state & PARSE_HAS_ENDPOINT_INT) &&
                            (parse_state & PARSE_HAS_MAC_ADDR))
                            goto init_success; // if we have both, we are done -- hard exit
                    }
                    else if (parse_state & PARSE_HAS_CONTROL_IF)
                    {
                        endpoint_addr.interrupt = endpoint->bEndpointAddress;
                        parse_state |= PARSE_HAS_ENDPOINT_INT;
                    }
                }
                    break;
                case USB_INTERFACE_DESCRIPTOR:
                    // we should look for this to either:
                    // (1) find the CDC Control Class/ECM interface, or
                    // (2) find the Interface number that matches the Interface indicated by the USB_CS_INTERFACE descriptor
                {
                    // cast to struct of type interface descriptor
                    usb_interface_descriptor_t *iface = (usb_interface_descriptor_t *)desc;
                    // if we have a control interface and ifnum is non-zero (we have an interface num to look for)
                    if (parse_state & PARSE_HAS_BULK_IF_NUM)
                    {
                        if ((iface->bInterfaceNumber == ifnum) &&
                            (iface->bNumEndpoints == 2) &&
                            (iface->bInterfaceClass == USB_CDC_DATA_CLASS))
                        {
                            if_bulk.addr = iface;
                            if_bulk.len = desc_len - parsed_len;
                            parse_state |= PARSE_HAS_BULK_IF;
                        }
                    }
                    else
                    {
                        // if we encounter another interface type after a control interface that isn't the CS_INTERFACE
                        // then we don't have the correct interface. This could be a malformed descriptor or something else
                        parse_state = 0; // reset parser state
                        
                        // If the interface is class CDC control and subtype ECM, this might be the correct interface union
                        // the next thing we should encounter is see case USB_CS_INTERFACE_DESCRIPTOR
                        if ((iface->bInterfaceClass == USB_COMM_CLASS) &&
                            ((iface->bInterfaceSubClass == USB_ECM_SUBCLASS) ||
                             (iface->bInterfaceSubClass == USB_NCM_SUBCLASS)))
                        {
                            // use this to set configuration
                            configdata.addr = &descriptor.conf;
                            configdata.len = desc_len;
                            tmp.type = iface->bInterfaceSubClass;
                            if (usb_SetConfiguration(device, configdata.addr, configdata.len))
                                return false;
                            parse_state |= PARSE_HAS_CONTROL_IF;
                        }
                    }
                }
                    break;
                case USB_CS_INTERFACE_DESCRIPTOR:
                    // this is a class-specific descriptor that specifies the interfaces used by the control interface
                {
                    usb_cs_interface_descriptor_t *cs = (usb_cs_interface_descriptor_t *)desc;
                    switch (cs->bDescriptorSubType)
                    {
                        case USB_ETHERNET_FUNCTIONAL_DESCRIPTOR:
                        {
                            usb_ethernet_functional_descriptor_t *ethdesc = (usb_ethernet_functional_descriptor_t *)cs;
                            usb_control_setup_t get_mac_addr = {0b10100001, REQUEST_GET_NET_ADDRESS, 0, 0, 6};
                            size_t xferd_tmp;
                            uint8_t string_descriptor_buf[DESCRIPTOR_MAX_LEN];
                            usb_string_descriptor_t *macaddr = (usb_string_descriptor_t *)string_descriptor_buf;
                            // Get MAC address and save for lwIP use (ETHARP header)
                            // if index for iMacAddress valid and GetStringDescriptor success, save MAC address
                            // else attempt control transfer to get mac address and save it
                            // else generate random compliant local MAC address and send control request to set the hwaddr, then save it
                            if (ethdesc->iMacAddress &&
                                (!usb_GetStringDescriptor(device, ethdesc->iMacAddress, 0, macaddr, DESCRIPTOR_MAX_LEN, &xferd_tmp)))
                            {
                                for (uint24_t i = 0; i < NETIF_MAX_HWADDR_LEN; i++)
                                    tmp.hwaddr[i] = (nibble(macaddr->bString[2 * i]) << 4) | nibble(macaddr->bString[2 * i + 1]);
                                
                                parse_state |= PARSE_HAS_MAC_ADDR;
                            }
                            else if (!usb_DefaultControlTransfer(device, &get_mac_addr, &tmp.hwaddr[0], eth_conf.max_retries, &xferd_tmp))
                            {
                                parse_state |= PARSE_HAS_MAC_ADDR;
                            }
                            else
                            {
#define RMAC_RANDOM_MAX 0xFFFFFF
                                usb_control_setup_t set_mac_addr = {0b00100001, REQUEST_SET_NET_ADDRESS, 0, 0, 6};
                                uint24_t rmac[2];
                                rmac[0] = (uint24_t)(random() & RMAC_RANDOM_MAX);
                                rmac[1] = (uint24_t)(random() & RMAC_RANDOM_MAX);
                                memcpy(&tmp.hwaddr[0], rmac, 6);
                                tmp.hwaddr[0] &= 0xFE;
                                tmp.hwaddr[0] |= 0x02;
                                if (!usb_DefaultControlTransfer(eth->device, &set_mac_addr, &tmp.hwaddr[0], eth_conf.max_retries, &xferd_tmp))
                                {
                                    parse_state |= PARSE_HAS_MAC_ADDR;
                                }
                            }
                        }
                            break;
                        case USB_UNION_FUNCTIONAL_DESCRIPTOR:
                        {
                            // if union functional type, this contains interface number for bulk transfer
                            usb_union_functional_descriptor_t *func = (usb_union_functional_descriptor_t *)cs;
                            ifnum = func->bInterface;
                            parse_state |= PARSE_HAS_BULK_IF_NUM;
                        }
                            break;
                        case USB_NCM_FUNCTIONAL_DESCRIPTOR:
                        {
                            usb_ncm_functional_descriptor_t *ncm = (usb_ncm_functional_descriptor_t *)cs;
                            tmp.class.ncm.bm_capabilities = ncm->bmNetworkCapabilities;
                        }
                            break;
                    }
                }
            }
            parsed_len += desc->bLength;
            desc = (usb_descriptor_t *)(((uint8_t *)desc) + desc->bLength);
        }
    }
    return false;
init_success:
    if (ncm_control_setup(&tmp))
        return false;
    
    tmp.rx.callback = (tmp.type == USB_NCM_SUBCLASS)   ? ncm_receive_callback
    : (tmp.type == USB_ECM_SUBCLASS) ? ecm_receive_callback
    : NULL;
    
    tmp.tx.emit = (tmp.type == USB_NCM_SUBCLASS)   ? ncm_bulk_transmit
    : (tmp.type == USB_ECM_SUBCLASS) ? ecm_bulk_transmit
    : NULL;
    
    if ((tmp.tx.emit == NULL) || (tmp.rx.callback == NULL))
        return false;
    
    // switch to alternate interface
    if (usb_SetInterface(device, if_bulk.addr, if_bulk.len))
        return false;
    
    // set endpoint data
    tmp.rx.endpoint = usb_GetDeviceEndpoint(device, endpoint_addr.in);
    tmp.tx.endpoint = usb_GetDeviceEndpoint(device, endpoint_addr.out);
    tmp.interrupt.endpoint = usb_GetDeviceEndpoint(device, endpoint_addr.interrupt);
    // allocate eth_device_t => contains type, usb device, metadata, and INT/RX buffers
    
    // better ifnum assignment
    uint8_t ifnum_assigned;
    for (ifnum_assigned = 0; ifnum_assigned < NETIFS_MAX_ALLOWED; ifnum_assigned++)
        if (!CHECK_BIT(ifnums_used, ifnum_assigned))
            break;
    
#if ETH_DEBUG
    if(ifnum_assigned == NETIFS_MAX_ALLOWED){
        // this is a bug because code earlier should detect this condition
        LWIP_DEBUGF(ETH_DEBUG | LWIP_DBG_LEVEL_SERIOUS,
                    ("ERROR: Interface assign err. This is a bug. File an issue on https://github.com/cagstech/lwip-ce with the following:\nusb_ethernet.c:731 ifs_used=%u, if_assign=%u", ifnums_used, ifnum_assigned));
        return false;
    }
#endif
    
    if(usb_GetDeviceData(device)){
        // ## IF DEVICE ALREADY USED FOR NETIF ##
        // reuse existing eth_device_t address
        eth = (eth_device_t *)usb_GetDeviceData(device);
        // copy new usb config without destroying netif config
        memcpy(eth, &tmp, sizeof(eth_device_t) - sizeof(struct netif));
        LWIP_DEBUGF(ETH_DEBUG | LWIP_DBG_STATE,
                    ("INFO: netif=%c%c%u <- device=%p, RESUMED", eth->iface.name[0], eth->iface.name[1], eth->iface.num, device));
        eth_netif_init(&eth->iface);
    }
    else {
        // ## ELSE CONFIG NEW NETIF ##
        if((eth = malloc(sizeof(eth_device_t)))==NULL)
            return false;
        memcpy(eth, &tmp, sizeof(eth_device_t));
        struct netif *iface = &eth->iface;
        // add to lwIP list of active netifs (save pointer to eth_device_t too)
        if (netif_add_noaddr(iface, eth, eth_netif_init, netif_input) == NULL)
        {
            LWIP_DEBUGF(ETH_DEBUG | LWIP_DBG_LEVEL_SERIOUS,
                        ("ERROR: netif= <- device=%p, netif add failed", device));
            free(eth);
            return false;
        }
        // fetch next available device number to use
        // set pointer to eth_device_t as associated data for usb device too
        usb_SetDeviceData(device, eth);
        
        iface->name[0] = 'e';
        iface->name[1] = 'n';
        
        iface->num = ifnum_assigned;         // use IFnum that triggered break
        
        // allow IPv4 and IPv6 on device
        netif_create_ip6_linklocal_address(iface, 1);
        iface->ip6_autoconfig_enabled = 1;
        
        ifnums_used |= 1 << ifnum_assigned;  // set flag marking the ifnum used
        netif_set_hostname(iface, hostname); // set default hostname
        LWIP_DEBUGF(ETH_DEBUG | LWIP_DBG_STATE,
                    ("INFO: netif=%c%c%u <- device=%p, CREATED", iface->name[0], iface->name[1], iface->num, device));
    }
    
    netif_set_up(&eth->iface); // tell lwIP that the interface is ready to receive
    // enqueue callbacks for receiving interrupt and RX transfers from this device.
    usb_ScheduleInterruptTransfer(eth->interrupt.endpoint, eth->interrupt.buf, INTERRUPT_RX_MAX, interrupt_receive_callback, eth);
    usb_ScheduleBulkTransfer(eth->rx.endpoint, eth->rx.buf, (tmp.type == USB_NCM_SUBCLASS) ? NCM_RX_NTB_MAX_SIZE : ETHERNET_MTU, eth->rx.callback, eth);
    return true;
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
            if(usb_GetDeviceFlags(usb_device) & USB_IS_HUB){
                // add handling for hubs
                union
                {
                    uint8_t bytes[DESCRIPTOR_MAX_LEN];   // allocate 256 bytes for descriptors
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
            if (init_ethernet_usb_device(usb_device))
                break;
            break;
        case USB_DEVICE_DISCONNECTED_EVENT:
        case USB_DEVICE_DISABLED_EVENT:
        {
            eth_device_t *eth_device = (eth_device_t *)usb_GetDeviceData(usb_device);
            netif_set_link_down(&eth_device->iface);
            netif_set_down(&eth_device->iface);
            if (eth_device)
            {
                eth_device_teardown(eth_device);
                LWIP_DEBUGF(ETH_DEBUG | LWIP_DBG_LEVEL_SEVERE,
                            ("INFO: device ptr=%p: disconnected", usb_device));
            }
        }
            if(event == USB_DEVICE_DISCONNECTED_EVENT)
                usb_UnrefDevice(usb_device);
            break;
        case USB_HOST_PORT_CONNECT_STATUS_CHANGE_INTERRUPT:
            return USB_ERROR_NO_DEVICE;
            break;
        default:
            break;
    }
    return USB_SUCCESS;
}

bool eth_configure(struct eth_configurator *conf){
    if(conf==NULL || (!conf->version))
        return false;
    if(conf->version > sizeof(struct eth_configurator)) {
        LWIP_DEBUGF(ETH_DEBUG | LWIP_DBG_LEVEL_WARNING,
                    ("Newer configuration passed than supported. Some options will have no effect."));
    }
    memcpy(&eth_conf, conf, LWIP_MIN(conf->version, sizeof(struct eth_configurator)));
    return true;
}


uint8_t eth_get_interfaces(void)
{
    return ifnums_used;
}
