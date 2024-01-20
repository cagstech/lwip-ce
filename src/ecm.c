/**
 * CDC "Ethernet Control Model (ECM)" driver code for integration with lwIP
 * author: acagliano
 * platform: TI-84+ CE
 * license: GPL3
 */
#include <stdio.h>
#include <string.h>
#include <usbdrvce.h>
#include "include/lwip/netif.h"
#include "include/lwip/pbuf.h"
#include "lwip/stats.h"
#include "lwip/snmp.h"
#include "include/lwip/dhcp.h"
#include "include/lwip/ethip6.h"

#include "ecm.h"
struct netif ecm_netif = {0};

// class-specific descriptor common class
typedef struct
{
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bDescriptorSubType;
    uint8_t data[];
} usb_cs_interface_descriptor_t;

// descriptor type for interface selection
typedef struct
{
    uint8_t bFunctionLength;
    uint8_t bDescriptorType;
    uint8_t bDescriptorSubType;
    uint8_t bControlInterface;
    uint8_t bInterface;
} usb_union_functional_descriptor_t;

// descriptor type for ethernet data
typedef struct
{
    uint8_t bFunctionLength;
    uint8_t bDescriptorType;
    uint8_t bDescriptorSubType;
    uint8_t iMacAddress;
    uint8_t bmEthernetStatistics[4];
    uint8_t wMaxSegmentSize[2];
    uint8_t wNumberMCFilters[2];
    uint8_t bNumberPowerFilters;

} usb_ethernet_functional_descriptor_t;

uint8_t in_buf[ECM_MTU];
ecm_device_t ecm_device = {0};

enum _descriptor_parser_await_states
{
    PARSE_HAS_CONTROL_IF = 1,
    PARSE_HAS_MAC_ADDR = (1 << 1),
    PARSE_HAS_BULK_IF_NUM = (1 << 2),
    PARSE_HAS_BULK_IF = (1 << 3),
    PARSE_HAS_ENDPOINT_IN = (1 << 4),
    PARSE_HAS_ENDPOINT_OUT = (1 << 5)
};

void hexdump(uint8_t *hex, size_t len)
{
    for (size_t i = 0; i < len; i++)
    {
        printf("%02X", hex[i]);
        if ((i % 8 == 0) && (i != 0))
            printf("\n");
    }
}

static void
netif_status_callback(struct netif *netif)
{
    dbg_printf("netif status changed %s\n", ip4addr_ntoa(netif_ip4_addr(netif)));
}

err_t ecm_netif_init(struct netif *netif)
{
    netif->linkoutput = ecm_transmit;
    netif->output = etharp_output;
    netif->output_ip6 = ethip6_output;
    netif->mtu = ECM_MTU;
    netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_ETHERNET | NETIF_FLAG_IGMP | NETIF_FLAG_MLD6;
    MIB2_INIT_NETIF(netif, snmp_ifType_ethernet_csmacd, 100000000);
    memcpy(netif->hwaddr, ecm_device.usb.mac_addr, NETIF_MAX_HWADDR_LEN);
    netif->hwaddr_len = NETIF_MAX_HWADDR_LEN;
    return ERR_OK;
}

// Defines ECM-specific classes/subclasses not defined in <usbdrvce.h>
#define USB_ECM_SUBCLASS 0x06
#define USB_CS_INTERFACE_DESCRIPTOR 0x24
#define USB_UNION_FUNCTIONAL_DESCRIPTOR 0x06
#define USB_ETHERNET_FUNCTIONAL_DESCRIPTOR 0x0F
bool init_ecm_device(void)
{
    size_t xferd, parsed_len, desc_len;
    usb_error_t err;
    union
    {
        uint8_t bytes[256];                     // allocate 256 bytes for descriptors
        usb_descriptor_t desc;                  // descriptor type aliases
        usb_device_descriptor_t dev;            // .. device descriptor alias
        usb_configuration_descriptor_t conf;    // .. config descriptor alias
        usb_union_functional_descriptor_t func; // .. union/if-specific descriptor alias
        usb_ethernet_functional_descriptor_t eth;
    } descriptor;
    err = usb_GetDeviceDescriptor(ecm_device.usb.device, &descriptor.dev, sizeof(usb_device_descriptor_t), &xferd);
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
        desc_len = usb_GetConfigurationDescriptorTotalLength(ecm_device.usb.device, config);
        parsed_len = 0;
        if (desc_len > 256)
            // if we overflow buffer, skip descriptor
            continue;
        // fetch config descriptor
        err = usb_GetConfigurationDescriptor(ecm_device.usb.device, config, &descriptor.conf, desc_len, &xferd);
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
                // we should only look for this IF we have found the ECM control interface,
                // and have retrieved the bulk data interface number from the CS_INTERFACE descriptor
                // see case USB_CS_INTERFACE_DESCRIPTOR and case USB_INTERFACE_DESCRIPTOR
                if (parse_state & PARSE_HAS_BULK_IF)
                {
                    usb_endpoint_descriptor_t *endpoint = (usb_endpoint_descriptor_t *)desc;
                    if (endpoint->bEndpointAddress & (USB_DEVICE_TO_HOST))
                    {
                        ecm_device.usb.if_data.endpoint.addr_in = endpoint->bEndpointAddress; // set out endpoint address
                        parse_state |= PARSE_HAS_ENDPOINT_IN;
                    }
                    else
                    {
                        ecm_device.usb.if_data.endpoint.addr_out = endpoint->bEndpointAddress; // set in endpoint address
                        parse_state |= PARSE_HAS_ENDPOINT_OUT;
                    }
                    if ((parse_state & PARSE_HAS_ENDPOINT_IN) && (parse_state & PARSE_HAS_ENDPOINT_OUT))
                        goto init_success; // if we have both, we are done -- hard exit
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
                            ecm_device.usb.if_data.addr = iface;
                            ecm_device.usb.if_data.len = desc_len - parsed_len;
                            parse_state |= PARSE_HAS_BULK_IF;
                        }
                    }
                    else
                    {
                        // if we encounter another interface type after a control interface that isn't the CS_INTERFACE
                        // then we don't have the correct interface. This could be a malformed descriptor or something else
                        ecm_device.usb.if_control.addr = NULL; // reset control interface if ifnum is not set
                        parse_state = 0;                       // reset parser state

                        // If the interface is class CDC control and subtype ECM, this might be the correct interface union
                        // the next thing we should encounter is see case USB_CS_INTERFACE_DESCRIPTOR
                        if ((iface->bInterfaceClass == USB_COMM_CLASS) &&
                            (iface->bInterfaceSubClass == USB_ECM_SUBCLASS))
                        {
                            // this is our control interface. maybe.
                            // we probably don't need the control if data but we'll save it anyway
                            ecm_device.usb.if_control.addr = iface;
                            ecm_device.usb.if_control.len = desc_len - parsed_len;

                            // use this to set configuration
                            ecm_device.usb.config.addr = &descriptor.conf;
                            ecm_device.usb.config.len = desc_len;
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
                        usb_ethernet_functional_descriptor_t *eth = (usb_ethernet_functional_descriptor_t *)cs;
                        size_t xferd_tmp;
                        uint8_t desc_buf[8];
                        usb_string_descriptor_t *mac_addr = (usb_string_descriptor_t *)desc_buf;
                        err = usb_GetStringDescriptor(ecm_device.usb.device, eth->iMacAddress, 0, mac_addr, 8, &xferd_tmp);
                        if (!err)
                        {
                            memcpy(ecm_device.usb.mac_addr, mac_addr->bString, 6);
                            parse_state |= PARSE_HAS_MAC_ADDR;
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
                    }
                }
            }
            parsed_len += desc->bLength;
            desc = (usb_descriptor_t *)(((uint8_t *)desc) + desc->bLength);
        }
    }
    return false;
init_success:
    if (usb_SetConfiguration(ecm_device.usb.device, ecm_device.usb.config.addr, ecm_device.usb.config.len))
        return USB_ERROR_FAILED;
    if (usb_SetInterface(ecm_device.usb.device, ecm_device.usb.if_data.addr, ecm_device.usb.if_data.len))
        return USB_ERROR_FAILED;
    ecm_device.usb.if_data.endpoint.in = usb_GetDeviceEndpoint(ecm_device.usb.device, ecm_device.usb.if_data.endpoint.addr_in);
    ecm_device.usb.if_data.endpoint.out = usb_GetDeviceEndpoint(ecm_device.usb.device, ecm_device.usb.if_data.endpoint.addr_out);
    ecm_device.status = ECM_READY;
    return true;
}

usb_error_t
ecm_handle_usb_event(usb_event_t event, void *event_data,
                     usb_callback_data_t *callback_data)
{

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
    case USB_DEVICE_ENABLED_EVENT:
        ecm_device.usb.device = event_data;
        if (init_ecm_device())
        {
            ip4_addr_t addr = IPADDR4_INIT_BYTES(192, 168, 1, 79);
            ip4_addr_t netmask = IPADDR4_INIT_BYTES(255, 255, 255, 0);
            ip4_addr_t gateway = IPADDR4_INIT_BYTES(192, 168, 1, 1);
            if (netif_add(&ecm_netif, &addr, &netmask, &gateway, NULL, ecm_netif_init, netif_input))
                printf("netif added\n");
            ecm_netif.name[0] = 'e';
            ecm_netif.name[1] = 'n';
            ecm_netif.num = 0;
            netif_create_ip6_linklocal_address(&ecm_netif, 1);
            ecm_netif.ip6_autoconfig_enabled = 1;
            netif_set_status_callback(&ecm_netif, netif_status_callback);
            netif_set_hostname(&ecm_netif, "ti84pce");
            netif_set_default(&ecm_netif);
            netif_set_up(&ecm_netif);
            if (netif_is_up(&ecm_netif))
            {
                printf("netif is up\n");
                netif_set_link_up(&ecm_netif);
                ecm_receive();
                if (!dhcp_start(&ecm_netif))
                    printf("dhcp init ok\n");
            }
        }
        else
            printf("not an ecm device\n");
        break;
    case USB_DEVICE_RESUMED_EVENT:
        netif_set_up(&ecm_netif);
        netif_set_link_up(&ecm_netif);
        break;

    case USB_DEVICE_DISCONNECTED_EVENT:
    case USB_HOST_PORT_CONNECT_STATUS_CHANGE_INTERRUPT:
        netif_remove(&ecm_netif);
        return USB_ERROR_NO_DEVICE;

    case USB_DEVICE_SUSPENDED_EVENT:
    case USB_DEVICE_DISABLED_EVENT:
        netif_set_down(&ecm_netif);
        netif_set_link_down(&ecm_netif);
        break;
    }
    return USB_SUCCESS;
}

usb_error_t ecm_receive_callback(usb_endpoint_t endpoint,
                                 usb_transfer_status_t status,
                                 size_t transferred,
                                 usb_transfer_data_t *data)
{
    if (status & USB_TRANSFER_NO_DEVICE)
        return USB_ERROR_NO_DEVICE;
    if (transferred)
    {
        struct pbuf *p = pbuf_alloc(PBUF_RAW, transferred, PBUF_POOL);
        if (p != NULL)
        {
            LINK_STATS_INC(link.recv);
            MIB2_STATS_NETIF_ADD(&ecm_netif, ifinoctets, p->tot_len);
            pbuf_take(p, in_buf, transferred);
            if (ecm_netif.input(p, &ecm_netif) != ERR_OK)
                pbuf_free(p);
        }
        else
            printf("pbuf alloc error");
    }
    return usb_ScheduleBulkTransfer(ecm_device.usb.if_data.endpoint.in, in_buf, ECM_MTU, ecm_receive_callback, NULL);
}

usb_error_t ecm_receive(void)
{
    return usb_ScheduleBulkTransfer(ecm_device.usb.if_data.endpoint.in, in_buf, ECM_MTU, ecm_receive_callback, NULL);
}

usb_error_t ecm_transmit_callback(usb_endpoint_t endpoint,
                                  usb_transfer_status_t status,
                                  size_t transferred,
                                  usb_transfer_data_t *data)
{
    return USB_SUCCESS;
}

usb_error_t ecm_transmit(struct netif *netif, struct pbuf *p)
{
    if (p->tot_len <= ECM_MTU)
    {
        LINK_STATS_INC(link.xmit);
        // Update SNMP stats(only if you use SNMP)
        MIB2_STATS_NETIF_ADD(netif, ifoutoctets, p->tot_len);
        uint8_t tbuf[p->tot_len];
        uint8_t *obuf = pbuf_get_contiguous(p, tbuf, sizeof(tbuf), p->tot_len, 0);
        return usb_BulkTransfer(ecm_device.usb.if_data.endpoint.out, obuf, p->tot_len, ecm_transmit_callback, NULL);
    }
}
