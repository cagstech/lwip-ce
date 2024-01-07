#include <usbdrvce.h>
// #include "lwip/netif.h"
// #include "lwip/pbuf.h"
// #include "lwip/stats.h"
// #include "lwip/snmp.h"

#include "ecm.h"

static u8_t buf[1518];
usb_device_t usb_device = NULL;
struct ecm_state ecm_state;

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

bool ecm_init(void)
{
    // validate that the device is actually a CDC-ECM device
    usb_device_descriptor_t *d;
    size_t len, xferd;
    // usb_GetDeviceDescriptor(usb_device, &d, &len, &xferd);
    usb_GetConfigurationDescriptor(usb_device, &d, &len, &xferd);
    // we'll actually code this later. for now, just trust the user
    // famous last words...
    // if(!ecm_device) return false;

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

struct pbuf *veth_receive(void)
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
    usb_ScheduleBulkTransfer(usb_endpoint_t endpoint, buf, p->tot_len,
                             usb_transfer_callback_t handler,
                             usb_transfer_data_t * data) return ERR_OK;
}
