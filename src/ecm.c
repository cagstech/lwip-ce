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
#include "include/lwip/timeouts.h"
#include "lwip/stats.h"
#include "lwip/snmp.h"
#include "lwip/dhcp.h"
#include "lwip/ethip6.h"
#include "lwip/dns.h"
#include "lwip/dhcp.h"
#include "lwip/etharp.h"

#include "ecm.h"
struct netif ecm_netif = {0};
ecm_device_t ecm = {0};
const char *hostname = "ti84pce";

uint8_t in_buf[ETHERNET_MTU];
uint8_t interrupt_buf[64];

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

void hexdump(uint8_t *hex, size_t len)
{
    for (size_t i = 0; i < len; i++)
    {
        if ((i % 16 == 0) && (i != 0))
            printf("\n");
        else if ((i % 8 == 0) && (i != 0))
            printf("  ");
        printf("%02X", hex[i]);
    }
    printf("\n\n");
}

uint8_t nibble(uint16_t c)
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

static void netif_link_callback(struct netif *netif)
{
    dhcp_start(netif);
    // dns_init();
}

static void
netif_status_callback(struct netif *netif)
{
    printf("netif status changed %s\n", ip4addr_ntoa(netif_ip4_addr(netif)));
}

usb_error_t ecm_bulk_receive_callback(usb_endpoint_t endpoint,
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
    return usb_ScheduleBulkTransfer(ecm.endpoint.in, in_buf, ETHERNET_MTU, ecm_bulk_receive_callback, NULL);
}

#define INTERRUPT_REQUEST_TYPE 0b10100001
#define INTERRUPT_NOTIFY_NETWORK_CONNECTION 0
usb_error_t ecm_interrupt_receive_callback(usb_endpoint_t endpoint,
                                           usb_transfer_status_t status,
                                           size_t transferred,
                                           usb_transfer_data_t *data)
{
    usb_control_setup_t *ctl = (usb_control_setup_t *)interrupt_buf;
    usb_control_setup_t *nc = (usb_control_setup_t *)(((uint8_t *)ctl) + ctl->wLength + sizeof(usb_control_setup_t));
    if (nc->bmRequestType == INTERRUPT_REQUEST_TYPE && nc->bRequest == INTERRUPT_NOTIFY_NETWORK_CONNECTION)
    {
        if (nc->wValue)
        {
            netif_set_up(&ecm_netif);
            netif_set_link_up(&ecm_netif);
            usb_ScheduleBulkTransfer(ecm.endpoint.in, in_buf, ETHERNET_MTU, ecm_bulk_receive_callback, NULL);
        }
        else
        {
            netif_set_link_down(&ecm_netif);
            netif_set_down(&ecm_netif);
        }
    }

    return usb_ScheduleInterruptTransfer(ecm.endpoint.interrupt, interrupt_buf, 64, ecm_interrupt_receive_callback, NULL);
}

usb_error_t ecm_bulk_transmit_callback(usb_endpoint_t endpoint,
                                       usb_transfer_status_t status,
                                       size_t transferred,
                                       usb_transfer_data_t *data)
{
    // Handle completion or error of the transfer, if needed
    if (status != USB_TRANSFER_COMPLETED)
    {
        return ERR_IF;
        // Handle error
    }
    return USB_SUCCESS;
}

uint8_t obuf[ETHERNET_MTU];
err_t ecm_bulk_transmit(struct netif *netif, struct pbuf *p)
{
    uint8_t *sendbuf = pbuf_get_contiguous(p, obuf, ETHERNET_MTU, p->tot_len, 0);
    if (sendbuf)
    {
        LINK_STATS_INC(link.xmit);
        // Update SNMP stats(only if you use SNMP)
        MIB2_STATS_NETIF_ADD(netif, ifoutoctets, p->tot_len);
        // hexdump(obuf, p->tot_len);
        printf("bulk xfer out\n");
        if (usb_BulkTransfer(ecm.endpoint.out, sendbuf, p->tot_len, ecm_bulk_transmit_callback, NULL) == USB_SUCCESS)
            return ERR_OK;
        return ERR_IF;
    }
    else
        return ERR_MEM;
}

err_t ecm_netif_init(struct netif *netif)
{
    netif->linkoutput = ecm_bulk_transmit;
    netif->output = etharp_output;
    netif->output_ip6 = ethip6_output;
    netif->mtu = ETHERNET_MTU;
    netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_ETHERNET | NETIF_FLAG_IGMP | NETIF_FLAG_MLD6;
    MIB2_INIT_NETIF(netif, snmp_ifType_ethernet_csmacd, 100000000);
    memcpy(netif->hwaddr, ecm.hwaddr, NETIF_MAX_HWADDR_LEN);
    netif->hwaddr_len = NETIF_MAX_HWADDR_LEN;
    printf("\n");
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
        uint8_t bytes[256];                  // allocate 256 bytes for descriptors
        usb_descriptor_t desc;               // descriptor type aliases
        usb_device_descriptor_t dev;         // .. device descriptor alias
        usb_configuration_descriptor_t conf; // .. config descriptor alias
    } descriptor;
    err = usb_GetDeviceDescriptor(ecm.device, &descriptor.dev, sizeof(usb_device_descriptor_t), &xferd);
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
        desc_len = usb_GetConfigurationDescriptorTotalLength(ecm.device, config);
        parsed_len = 0;
        if (desc_len > 256)
            // if we overflow buffer, skip descriptor
            continue;
        // fetch config descriptor
        err = usb_GetConfigurationDescriptor(ecm.device, config, &descriptor.conf, desc_len, &xferd);
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
                        ecm.mtu.in = endpoint->wMaxPacketSize;
                        parse_state |= PARSE_HAS_ENDPOINT_IN;
                    }
                    else
                    {
                        endpoint_addr.out = endpoint->bEndpointAddress; // set in endpoint address
                        ecm.mtu.out = endpoint->wMaxPacketSize;
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
                            (iface->bInterfaceSubClass == USB_ECM_SUBCLASS))
                        {
                            // use this to set configuration
                            configdata.addr = &descriptor.conf;
                            configdata.len = desc_len;
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
#define HWADDR_DESCRIPTOR_LEN (sizeof(usb_string_descriptor_t) + (sizeof(wchar_t) * 12))
                        uint8_t desc_buf[HWADDR_DESCRIPTOR_LEN] = {0};
                        usb_string_descriptor_t *mac_addr = (usb_string_descriptor_t *)desc_buf;
                        err = usb_GetStringDescriptor(ecm.device, eth->iMacAddress, 0, mac_addr, HWADDR_DESCRIPTOR_LEN, &xferd_tmp);
                        printf("hwaddr: ");
                        if (!err)
                        {
                            for (int i = 0; i < NETIF_MAX_HWADDR_LEN; i++)
                            {
                                ecm.hwaddr[i] = (nibble(mac_addr->bString[2 * i]) << 4) | nibble(mac_addr->bString[2 * i + 1]);
                                printf("%02X ", ecm.hwaddr[i]);
                            }
                            printf("\n");
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
    if (usb_SetConfiguration(ecm.device, configdata.addr, configdata.len))
        return USB_ERROR_FAILED;
    if (usb_SetInterface(ecm.device, if_bulk.addr, if_bulk.len))
        return USB_ERROR_FAILED;
    ecm.endpoint.in = usb_GetDeviceEndpoint(ecm.device, endpoint_addr.in);
    ecm.endpoint.out = usb_GetDeviceEndpoint(ecm.device, endpoint_addr.out);
    ecm.endpoint.interrupt = usb_GetDeviceEndpoint(ecm.device, endpoint_addr.interrupt);
    usb_RefDevice(ecm.device);
    ecm.status = ECM_READY;
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
        ecm.device = event_data;
        if (init_ecm_device())
        {
            if (netif_add(&ecm_netif, NULL, NULL, NULL, NULL, ecm_netif_init, netif_input))
                printf("netif added\n");
            ecm_netif.name[0] = 'e';
            ecm_netif.name[1] = 'n';
            ecm_netif.num = 0;
            // netif_create_ip6_linklocal_address(&ecm_netif, 1);
            // ecm_netif.ip6_autoconfig_enabled = 1;
            netif_set_status_callback(&ecm_netif, netif_status_callback);
            netif_set_link_callback(&ecm_netif, netif_link_callback);
            netif_set_default(&ecm_netif);
            netif_set_up(&ecm_netif);
            netif_set_hostname(&ecm_netif, hostname);
            usb_ScheduleInterruptTransfer(ecm.endpoint.interrupt, interrupt_buf, 64, ecm_interrupt_receive_callback, NULL);
        }
        else
            printf("not an ecm device\n");
        break;

    case USB_DEVICE_RESUMED_EVENT:
        usb_ScheduleInterruptTransfer(ecm.endpoint.interrupt, interrupt_buf, 64, ecm_interrupt_receive_callback, NULL);
        break;
    case USB_DEVICE_DISCONNECTED_EVENT:
    case USB_HOST_PORT_CONNECT_STATUS_CHANGE_INTERRUPT:
        netif_remove(&ecm_netif);
        return USB_ERROR_NO_DEVICE;
        break;
    }
    return USB_SUCCESS;
}
