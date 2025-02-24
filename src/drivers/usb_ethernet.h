/****************************************************************************
 * Header File for Communications Data Class (CDC)
 * for USB-Ethernet devices
 * Includes definitions for CDC-ECM and CDC-NCM
 */

#ifndef cdc_h
#define cdc_h

#include <stdint.h>
#include <ethdrvce.h>

#include "lwip/err.h"
#include "lwip/netif.h"

#define USB_CDC_MAX_RETRIES 3

/* Ethernet MTU - ECM & NCM */
#define ETHERNET_MTU 1518

/* Interrupt buffer size */
#define INTERRUPT_RX_MAX 64

/* NCM rx ntb size */
#define NCM_RX_NTB_MAX_SIZE 2048

/* USB CDC Ethernet Device Classes */
#define USB_ECM_SUBCLASS 0x06
#define USB_NCM_SUBCLASS 0x0D

extern eth_error_t (*lwip_send_fn)(struct eth_device *eth, struct eth_transfer_data *xfer);
extern eth_error_t (*lwip_open_dev_fn)(struct eth_device *eth,
                                usb_device_t usb_device,
                                eth_transfer_callback_t handle_rx,
                                eth_transfer_callback_t handle_tx,
                                eth_transfer_callback_t handle_int,
                                eth_error_callback_t handle_error
                                );
extern eth_error_t (*lwip_close_dev_fn)(struct eth_device *eth);

usb_error_t
eth_usb_event_callback(usb_event_t event, void *event_data,
                       __attribute__((unused)) usb_callback_data_t *callback_data);

#endif
