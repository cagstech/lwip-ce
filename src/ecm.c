#include <stdio.h>
#include <string.h>
#include <usbdrvce.h>
// #include "lwip/netif.h"
// #include "lwip/pbuf.h"
// #include "lwip/stats.h"
// #include "lwip/snmp.h"

#include "ecm.h"

// class specific descriptor definition
typedef struct
{
    uint8_t bFunctionLength;
    uint8_t bDescriptorType;
    uint8_t bDescriptorSubType;
    uint8_t bControlInterface;
    uint8_t bInterface;
} usb_union_functional_descriptor_t;

#define ECM_MTU 1518
static uint8_t in_buf[ECM_MTU];
static uint8_t out_buf[ECM_MTU];
ecm_device_t ecm_device = {0};

enum _descriptor_parser_await_states
{
    PARSE_AWAIT_CONTROL_IF,
    PARSE_AWAIT_CS_INTERFACE,
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
    case USB_DEVICE_RESUMED_EVENT:
    case USB_DEVICE_ENABLED_EVENT:
        printf("device enabled\n");
        ecm_device.usb_device = event_data;
        return ecm_init();
        break;
        // break;
    case USB_DEVICE_SUSPENDED_EVENT:
    case USB_DEVICE_DISABLED_EVENT:
    case USB_DEVICE_DISCONNECTED_EVENT:
        printf("device lost\n");
        memset(&ecm_device, 0, sizeof(ecm_device));
        return USB_SUCCESS;
    }
    return USB_SUCCESS;
}

// Defines ECM-specific classes/subclasses not defined in <usbdrvce.h>
#define USB_ECM_SUBCLASS 0x06
#define USB_CS_INTERFACE_DESCRIPTOR 0x24
#define USB_UNION_DESCRIPTOR 0x06
ecm_error_t ecm_init(void)
{
    union
    {
        uint8_t bytes[256];
        usb_descriptor_t desc;
        usb_device_descriptor_t dev;
        usb_configuration_descriptor_t conf;
        usb_union_functional_descriptor_t func;
    } descriptor;
    size_t xferd, parsed_len, desc_len;
    usb_error_t err;
    // wait for device to be enabled, request device descriptor
    err = usb_GetDeviceDescriptor(ecm_device.usb_device, &descriptor.dev, sizeof(usb_device_descriptor_t), &xferd);
    if (err || xferd == 0)
        return ECM_ERROR_INVALID_DESCRIPTOR;
    // test for composite device, which is what an ECM device usually is
    if (descriptor.dev.bDeviceClass != USB_INTERFACE_SPECIFIC_CLASS)
        return ECM_ERROR_INVALID_DEVICE;
    uint8_t num_configs = descriptor.dev.bNumConfigurations;
    // foreach configuration of device
    for (int c_idx = 0; c_idx < num_configs; c_idx++)
    {
        // get total length of configuration descriptor, error if too large
        uint8_t ifnum = 0;
        uint8_t parse_state = PARSE_AWAIT_CONTROL_IF;
        desc_len = usb_GetConfigurationDescriptorTotalLength(ecm_device.usb_device, c_idx);
        parsed_len = 0;
        if (desc_len > 256)
            return ECM_ERROR_MEMORY;

        // request configuration descriptor(s)
        err = usb_GetConfigurationDescriptor(ecm_device.usb_device, c_idx, &descriptor.conf, desc_len, &xferd);
        if (err || xferd == 0)
            continue;

        usb_descriptor_t *desc = &descriptor.desc; // current working descriptor
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
                    if (endpoint->bEndpointAddress & 0b10000000)
                        ecm_device.in = usb_GetDeviceEndpoint(ecm_device.usb_device, endpoint->bEndpointAddress); // set out endpoint address
                    else
                        ecm_device.out = usb_GetDeviceEndpoint(ecm_device.usb_device, endpoint->bEndpointAddress); // set in endpoint address
                    if (ecm_device.in && ecm_device.out)
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
                            ecm_device.if_data.addr = iface;
                            ecm_device.if_data.len = desc_len - parsed_len;
                            if (usb_SetInterface(ecm_device.usb_device, ecm_device.if_data.addr, ecm_device.if_data.len))
                                return ECM_INTERFACE_ERROR;
                            parse_state = PARSE_AWAIT_ENDPOINTS;
                        }
                    }
                    else
                    {
                        // if we encounter another interface type after a control interface that isn't the CS_INTERFACE
                        // then we don't have the correct interface. This could be a malformed descriptor or something else
                        ecm_device.if_control = NULL;         // reset control interface if ifnum is not set
                        parse_state = PARSE_AWAIT_CONTROL_IF; // reset parser state

                        // If the interface is class CDC control and subtype ECM, this might be the correct interface union
                        // the next thing we should encounter is see case USB_CS_INTERFACE_DESCRIPTOR
                        if ((iface->bInterfaceClass == USB_COMM_CLASS) &&
                            (iface->bInterfaceSubClass == USB_ECM_SUBCLASS))
                        {
                            // this is our control interface. maybe.
                            if (usb_SetConfiguration(ecm_device.usb_device, &descriptor.conf, desc_len))
                                return ECM_CONFIGURATION_ERROR;
                            ecm_device.if_control = iface;
                            parse_state = PARSE_AWAIT_CS_INTERFACE;
                        }
                    }
                }
                break;
            case USB_CS_INTERFACE_DESCRIPTOR:
                // this is a class-specific descriptor that specifies the interfaces used by the control interface
                {
                    usb_union_functional_descriptor_t *func = (usb_union_functional_descriptor_t *)desc;
                    if (parse_state == PARSE_AWAIT_CS_INTERFACE)
                    {
                        if ((func->bDescriptorType == USB_CS_INTERFACE_DESCRIPTOR) &&
                            (func->bDescriptorSubType == USB_UNION_DESCRIPTOR))
                        {
                            // if this is a CS_INTERFACE class and union subtype, then bInterface is our interface
                            // number for bulk endpoints
                            ifnum = func->bInterface;
                            parse_state = PARSE_AWAIT_BULK_IF;
                        }
                    }
                }
                break;
            }
            parsed_len += desc->bLength;
            desc = (usb_descriptor_t *)(((uint8_t *)desc) + desc->bLength);
        }
    }
    return ECM_ERROR_NO_DEVICE;
init_success:
    ecm_device.ready = true;
    return ECM_OK;
}
bool transfer_fail = 0;
usb_error_t ecm_receive_callback(usb_endpoint_t endpoint,
                                 usb_transfer_status_t status,
                                 size_t transferred,
                                 usb_transfer_data_t *data)
{
    if (transferred != 0)
        printf("%u bytes received!\n", transferred);
    if (status & USB_TRANSFER_NO_DEVICE)
    {
        printf("transfer error");
        transfer_fail = 1;
        return USB_ERROR_NO_DEVICE;
    }
    return usb_ScheduleBulkTransfer(ecm_device.in, in_buf, ECM_MTU, ecm_receive_callback, NULL);
}

ecm_error_t ecm_receive(void)
{
    return usb_ScheduleBulkTransfer(ecm_device.in, in_buf, ECM_MTU, ecm_receive_callback, NULL);
}

usb_error_t ecm_transmit_callback(usb_endpoint_t endpoint,
                                  usb_transfer_status_t status,
                                  size_t transferred,
                                  usb_transfer_data_t *data)
{
    printf("%u bytes sent\n", transferred);
    return 0;
}

ecm_error_t ecm_transmit(void *buf, size_t len)
{
    // LINK_STATS_INC(link.xmit);
    /* Update SNMP stats (only if you use SNMP) */
    // MIB2_STATS_NETIF_ADD(netif, ifoutoctets, p->tot_len);
    // pbuf_copy_partial(p, buf, p->tot_len, 0);
    memcpy(out_buf, buf, len);
    if (ecm_device.usb_device)
        return usb_ScheduleBulkTransfer(ecm_device.out, out_buf, len, ecm_transmit_callback, NULL);
    return USB_ERROR_NO_DEVICE;
}
