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

#include "ecm.h"

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
uint8_t out_buf[ECM_MTU];
ecm_device_t ecm_device = {0};

enum _descriptor_parser_await_states
{
    PARSE_AWAIT_CONTROL_IF,
    PARSE_AWAIT_CS_INTERFACE,
    PARSE_AWAIT_MAC_ADDR,
    PARSE_AWAIT_BULK_IF,
    PARSE_AWAIT_ENDPOINTS,
    PARSE_OK
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

uint8_t nibble(uint8_t c)
{
    c -= 0x30;
    if (c < 10)
        return c;
    c -= 0x11;
    if (c < 6)
        return c + 10;
    c -= 0x20;
    if (c < 6)
        return c + 10;
    return 0xff;
}

usb_error_t
ecm_handle_usb_event(usb_event_t event, void *event_data,
                     usb_callback_data_t *callback_data)
{

    /* Enable newly connected devices */
    switch (event)
    {
    case USB_DEVICE_CONNECTED_EVENT:
        printf("device connected\n");
        if (!(usb_GetRole() & USB_ROLE_DEVICE))
        {
            usb_device_t usb_device = event_data;
            usb_ResetDevice(usb_device);
        }
        break;
    case USB_DEVICE_ENABLED_EVENT:
        printf("device enabled\n");
        ecm_device.usb.device = event_data;
        return ecm_init();
    case USB_DEVICE_RESUMED_EVENT:
        break;
    case USB_DEVICE_DISCONNECTED_EVENT:
    case USB_HOST_PORT_CONNECT_STATUS_CHANGE_INTERRUPT:
    case USB_DEVICE_SUSPENDED_EVENT:
    case USB_DEVICE_DISABLED_EVENT:
        printf("device lost\n");
        return USB_ERROR_NO_DEVICE;
    }
    return USB_SUCCESS;
}

// Defines ECM-specific classes/subclasses not defined in <usbdrvce.h>
#define USB_ECM_SUBCLASS 0x06
#define USB_CS_INTERFACE_DESCRIPTOR 0x24
#define USB_UNION_FUNCTIONAL_DESCRIPTOR 0x06
#define USB_ETHERNET_FUNCTIONAL_DESCRIPTOR 0x0F
usb_error_t ecm_init(void)
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
        return USB_ERROR_FAILED;

    // check for main DeviceClass being type 0x00 - composite/if-specific
    if (descriptor.dev.bDeviceClass != USB_INTERFACE_SPECIFIC_CLASS)
        return USB_ERROR_NOT_SUPPORTED;

    // parse both configs for the correct one
    uint8_t num_configs = descriptor.dev.bNumConfigurations;
    for (uint8_t config = 0; config < num_configs; config++)
    {
        uint8_t ifnum = 0;
        uint8_t parse_state = PARSE_AWAIT_CONTROL_IF;
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
                if (parse_state == PARSE_AWAIT_ENDPOINTS)
                {
                    usb_endpoint_descriptor_t *endpoint = (usb_endpoint_descriptor_t *)desc;
                    if (endpoint->bEndpointAddress & (USB_DEVICE_TO_HOST))
                        ecm_device.usb.if_data.endpoint.addr_in = endpoint->bEndpointAddress; // set out endpoint address
                    else
                        ecm_device.usb.if_data.endpoint.addr_out = endpoint->bEndpointAddress; // set in endpoint address
                    if ((ecm_device.usb.if_data.endpoint.addr_out) &&
                        (ecm_device.usb.if_data.endpoint.addr_in) &&
                        (ecm_device.usb.config.addr) &&
                        (ecm_device.usb.if_data.addr))
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
                    if (parse_state == PARSE_AWAIT_BULK_IF)
                    {
                        if ((iface->bInterfaceNumber == ifnum) &&
                            (iface->bNumEndpoints == 2) &&
                            (iface->bInterfaceClass == USB_CDC_DATA_CLASS))
                        {
                            ecm_device.usb.if_data.addr = iface;
                            ecm_device.usb.if_data.len = desc_len - parsed_len;
                            parse_state = PARSE_AWAIT_ENDPOINTS;
                        }
                    }
                    else
                    {
                        // if we encounter another interface type after a control interface that isn't the CS_INTERFACE
                        // then we don't have the correct interface. This could be a malformed descriptor or something else
                        ecm_device.usb.if_control.addr = NULL; // reset control interface if ifnum is not set
                        parse_state = PARSE_AWAIT_CONTROL_IF;  // reset parser state

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
                            parse_state = PARSE_AWAIT_CS_INTERFACE;
                        }
                    }
                }
                break;
            case USB_CS_INTERFACE_DESCRIPTOR:
                // this is a class-specific descriptor that specifies the interfaces used by the control interface
                {
                    if ((parse_state == PARSE_AWAIT_CS_INTERFACE) || (parse_state == PARSE_AWAIT_MAC_ADDR))
                    {
                        usb_cs_interface_descriptor_t *cs = (usb_cs_interface_descriptor_t *)desc;
                        if (cs->bDescriptorSubType == USB_ETHERNET_FUNCTIONAL_DESCRIPTOR)
                        {
                            // if ethernet functional type, contains ethernet configuration data
                            usb_ethernet_functional_descriptor_t *eth = (usb_ethernet_functional_descriptor_t *)cs;
                            size_t xferd_tmp;
                            uint8_t mac_str[12];
                            usb_GetStringDescriptor(ecm_device.usb.device, eth->iMacAddress, 0,
                                                    mac_str, 12, &xferd_tmp);
                            // convert to bytearray and save to ecm_device.usb.mac_addr
                            for (int i = 0; i < 6; i++)
                                ecm_device.usb.mac_addr[i] = nibble(mac_str[2 * i]) << 4 | nibble(mac_str[2 * i + 1]);
                        }
                        else if (cs->bDescriptorSubType == USB_UNION_FUNCTIONAL_DESCRIPTOR)
                        {
                            // if union functional type, this contains interface number for bulk transfer
                            usb_union_functional_descriptor_t *func = (usb_union_functional_descriptor_t *)cs;
                            ifnum = func->bInterface;
                            parse_state = PARSE_AWAIT_BULK_IF;
                        }
                    }
                    break;
                }
                parsed_len += desc->bLength;
                desc = (usb_descriptor_t *)(((uint8_t *)desc) + desc->bLength);
            }
        }
    }
    return USB_ERROR_NO_DEVICE;
init_success:
    if (usb_SetConfiguration(ecm_device.usb.device, ecm_device.usb.config.addr, ecm_device.usb.config.len))
        return USB_ERROR_FAILED;
    if (usb_SetInterface(ecm_device.usb.device, ecm_device.usb.if_data.addr, ecm_device.usb.if_data.len))
        return USB_ERROR_FAILED;
    ecm_device.usb.if_data.endpoint.in = usb_GetDeviceEndpoint(ecm_device.usb.device, ecm_device.usb.if_data.endpoint.addr_in);
    ecm_device.usb.if_data.endpoint.out = usb_GetDeviceEndpoint(ecm_device.usb.device, ecm_device.usb.if_data.endpoint.addr_out);
    ecm_device.status = ECM_READY;
    return USB_SUCCESS;
}

usb_error_t ecm_receive_callback(usb_endpoint_t endpoint,
                                 usb_transfer_status_t status,
                                 size_t transferred,
                                 usb_transfer_data_t *data)
{
    if (status & USB_TRANSFER_NO_DEVICE)
    {
        printf("transfer error\n");
        return USB_ERROR_NO_DEVICE;
    }
    if (transferred)
    {
        printf("packet received, %u bytes\n", transferred);
        struct pbuf *p = pbuf_alloc(PBUF_RAW, transferred, PBUF_POOL);
        if (p != NULL)
        {
            pbuf_take(p, in_buf, transferred);
            if (ecm_device.netif.input(p, &ecm_device.netif) != ERR_OK)
                pbuf_free(p);
        }
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
    printf("%u bytes sent\n", transferred);
    return USB_SUCCESS;
}

usb_error_t ecm_transmit(struct netif *netif, struct pbuf *p)
{
    LINK_STATS_INC(link.xmit);
    // Update SNMP stats(only if you use SNMP)
    MIB2_STATS_NETIF_ADD(netif, ifoutoctets, p->tot_len);
    pbuf_copy_partial(p, out_buf, p->tot_len, 0);
    printf("sending %u bytes\n", p->tot_len);
    return usb_ScheduleBulkTransfer(ecm_device.usb.if_data.endpoint.out, out_buf, p->tot_len, ecm_transmit_callback, NULL);
}
