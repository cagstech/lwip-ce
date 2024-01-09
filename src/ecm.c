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

static u8_t buf[1518];
usb_device_t usb_device = NULL;
struct ecm_state ecm_state = {0};

#define ECM_ENDPOINT_IN 0x81
#define ECM_ENDPOINT_OUT 0x02
#define USB_CS_INTERFACE_DESCRIPTOR 0x24
#define ECM_UNION_DESCRIPTOR 0x06

enum _descriptor_parser_await_states
{
    PARSE_AWAIT_CONTROL_IF,
    PARSE_AWAIT_CS_INTERFACE,
    PARSE_AWAIT_BULK_IF,
    PARSE_AWAIT_ENDPOINTS,
    PARSE_OK
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
    case USB_HOST_CONFIGURE_EVENT:
    {
        usb_device_t host = usb_FindDevice(NULL, NULL, USB_SKIP_HUBS);
        if (host)
            usb_device = host;
        if (ecm_init())
            return USB_SUCCESS;
        break;
    }
        // break;
    case USB_DEVICE_RESUMED_EVENT:
    case USB_DEVICE_ENABLED_EVENT:
        usb_device = event_data;
        if (ecm_init())
            return USB_SUCCESS;
        break;
        // break;
    case USB_DEVICE_SUSPENDED_EVENT:
    case USB_DEVICE_DISABLED_EVENT:
    case USB_DEVICE_DISCONNECTED_EVENT:
        return USB_SUCCESS;
    }
    return USB_SUCCESS;
}

#define CDC_CONTROL_CLASS 0x02
#define CDC_DATA_CLASS 0x0A
#define ECM_CLASS 0x06
#define ECM_PROTOCOL 0
bool ecm_init(void)
{
    // validate that the device is actually a CDC-ECM device
    union
    {
        uint8_t bytes[256];
        usb_configuration_descriptor_t conf;
        usb_device_descriptor_t device;
    } descriptor;
    size_t len, xferd;

    // wait for device to be enabled, request device descriptor
    if (usb_GetDeviceDescriptor(usb_device, &descriptor, &len, &xferd))
        return false;
    // test for composite device, which is what an ECM device usually is
    if (d->bDeviceClass != 0x00)
        return false;
    uint8_t num_configs = descriptor.device.bNumConfigurations;
    // foreach configuration of device
    for (int c_idx = 0; c_idx < num_configs; c_idx++)
    {
        // get total length of configuration descriptor, error if too large
        uint8_t ifnum = 0;
        uint8_t parse_state = PARSE_AWAIT_CONTROL_IF;
        size_t desc_len = usb_GetConfigurationDescriptorTotalLength(usb_device, c_idx),
               parsed_len = 0;
        if (desc_len > 256)
            return ECM_MEMORY_OVERFLOW;

        // request configuration descriptor(s)
        if (usb_GetConfigurationDescriptor(usb_device, c_idx, &descriptor, &len, &xferd))
            continue;
        usb_descriptor_t *desc = (usb_descriptor_t *)descriptor; // current working descriptor
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
                        ecm_state.ecm_out = endpoint->bEndpointAddress; // set out endpoint address
                    else
                        ecm_state.ecm_in = endpoint->bEndpointAddress; // set in endpoint address
                    if (ecm_state.ecm_in && ecm_state.ecm_out)
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
                            (iface->bInterfaceClass == CDC_DATA_CLASS))
                        {
                            ecm_state.if_data = iface;
                            parse_state = PARSE_AWAIT_ENDPOINTS;
                        }
                    }
                    else
                    {
                        // if we encounter another interface type after a control interface that isn't the CS_INTERFACE
                        // then we don't have the correct interface. This could be a malformed descriptor or something else
                        ecm_state.if_control = NULL;          // reset control interface if ifnum is not set
                        parse_state = PARSE_AWAIT_CONTROL_IF; // reset parser state

                        // If the interface is class CDC control and subtype ECM, this might be the correct interface union
                        // the next thing we should encounter is see case USB_CS_INTERFACE_DESCRIPTOR
                        if ((iface->bInterfaceClass == CDC_CONTROL_CLASS) &&
                            (iface->bInterfaceSubClass == ECM_CLASS))
                        {
                            // this is our control interface. maybe.
                            ecm_state.if_control = iface;
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
                            (func->bDescriptorSubType == ECM_UNION_DESCRIPTOR))
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

    return false;
init_success:
    if (usb_SetConfiguration(ecm_state.usb_device, descriptor, desc_len))
        return false;
    if (usb_SetInterface(ecm_state.usb_device, ecm_state.if_data, desc_len - parsed_len))
        return false;
    return true;
}
// setup netif in lwip

// ip4_addr_t addr = IPADDR4_INIT_BYTES(192, 168, 25, 2);
// ip4_addr_t netmask = IPADDR4_INIT_BYTES(255, 255, 255, 0);
// ip4_addr_t gateway = IPADDR4_INIT_BYTES(192, 168, 25, 1);
// netif_add(&netif, &addr, &netmask, &gateway, NULL, my_netif_init, netif_input);
// netif.name[0] = 'e';
// netif.name[1] = 'n';
// netif.num = 0;
// netif.state = usb_device;
// netif_create_ip6_linklocal_address(&netif, 1);
// netif.ip6_autoconfig_enabled = 1;
// netif_set_status_callback(&netif, netif_status_callback);
// netif_set_default(&netif);
// netif_set_up(&netif);
}

struct pbuf *
veth_receive(void)
{
    ssize_t size = veth_rx();
    if (size == -1)
    {
        return NULL;
    }
    struct pbuf *p = pbuf_alloc(PBUF_RAW, size, PBUF_POOL);
    if (p != NULL)
    {
        pbuf_take(p, buf, size);
    }
    return p;
}

err_t veth_transmit(struct netif *netif, struct pbuf *p)
{
    // LINK_STATS_INC(link.xmit);
    /* Update SNMP stats (only if you use SNMP) */
    // MIB2_STATS_NETIF_ADD(netif, ifoutoctets, p->tot_len);
    // pbuf_copy_partial(p, buf, p->tot_len, 0);
    usb_ScheduleBulkTransfer(ecm_state.ecm_out, buf, p->tot_len, NULL, NULL) return ERR_OK;
}
