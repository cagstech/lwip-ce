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
#define ECM_CS_INTERFACE 0x24
#define ECM_UNION_DESCRIPTOR 0x06

usb_error_t ecm_handle_usb_event(usb_event_t event, void *event_data,
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
    uint8_t if_data = 0;
    size_t len, xferd;

    // wait for device to be enabled, request device descriptor
    usb_GetDeviceDescriptor(usb_device, &descriptor, &len, &xferd);
    // test for composite device, which is what an ECM device usually is
    if (d->bDeviceClass != 0x00)
        return false;

    // foreach configuration of device
    for (int c_idx = 0; c_idx < descriptor.device.bNumConfigurations; c_idx++)
    {
        // get total length of configuration descriptor, error if too large
        size_t desc_len = usb_GetConfigurationDescriptorTotalLength(usb_device, c_idx), parsed_len = 0;
        if (desc_len > 256)
            return ECM_MEMORY_OVERFLOW;

        // request configuration descriptor(s)
        usb_GetConfigurationDescriptor(usb_device, c_idx, &descriptor, &len, &xferd);
        usb_descriptor_t *desc = (usb_descriptor_t *)descriptor; // current working descriptor
        while (parsed_len < desc_len)
        {
            if (ecm_state.if_data && ecm_state.if_control && (desc->bDescriptorType == USB_ENDPOINT_DESCRIPTOR))
            {
                usb_endpoint_descriptor_t *endpoint = (usb_endpoint_descriptor_t *)desc;
                if (endpoint->bEndpointAddress & 0b10000000)
                    ecm_state.ecm_out = endpoint->bEndpointAddress;
                else
                    ecm_state.ecm_in = endpoint->bEndpointAddress;
                if (ecm_state.ecm_in && ecm_state.ecm_out)
                    goto init_success;
            }
            else if (desc->bDescriptorType == USB_INTERFACE_DESCRIPTOR)
            { // only parse interface descriptors, cast to correct struct type
                usb_interface_descriptor_t *iface = (usb_interface_descriptor_t *)desc;
                if (ecm_state.if_control && if_data)
                {
                    if ((iface->bInterfaceNumber == if_data) &&
                        (iface->bNumEndpoints == 2) &&
                        (iface->bInterfaceClass == CDC_DATA_CLASS))
                        ecm_state.if_data = iface;
                }
                else
                {
                    ecm_state.if_control = NULL;

                    if ((iface->bInterfaceClass == CDC_CONTROL_CLASS) &&
                        (iface->bInterfaceSubClass == ECM_CLASS))
                        ecm_state.if_control = iface;
                }
            }
            else if (hasControlIf && desc->bDescriptorType == ECM_CS_INTERFACE)
            {
                usb_union_functional_descriptor_t *func = (usb_union_functional_descriptor_t *)desc;
                if (func->bDescriptorSubType == ECM_UNION_DESCRIPTOR)
                    dataif = func->bInterface;
            }
            parsed_len += desc->bLength;
            desc = (usb_descriptor_t *)(((uint8_t *)desc) + desc->bLength);
        }
    }
    return false;
init_success:
    usb_SetInterface(ecm_state.usb_device, ecm_state.if_data, size_t length); // um what the hell is length

    return true;
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
    //  veth_tx(buf, p->tot_len);
    usb_ScheduleBulkTransfer(ecm_state.ecm_out, buf, p->tot_len, NULL, NULL) return ERR_OK;
}
