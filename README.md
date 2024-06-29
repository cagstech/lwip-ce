# lwIP-CE #

## What are lwIP and lwIP-CE ##

**lwIP** is a full networking stack for low-resource device like embedded systems. It is maintained by non-GNU (https://github.com/lwip-tcpip/lwip).
This makes it perfect for something as ridiclous as a graphing calculator.
**lwIP-CE** is the name for the lwIP fork targetting the Texas Instruments TI-84+ CE graphing calculator.
To view the original README, open README-ORIG.md.

This implementation differs from the ported lwIP in the following ways:
- reduced pbuf pool and tcp_sndbuf queue size
- non "raw" API's will not work at due to NOSYS implementation
- hardware-specific USB CDC-ECM and NCM drivers

## Related Media ##
1. https://www.youtube.com/watch?v=fD2n7CzFeZU


# Basic Usage Instructions #

1. **Headers**: Users should include at minimum the following headers to use lwIP properly on this platform:
   
       #include <usbdrvce.h>                // usb drivers
       #include "drivers/usb-ethernet.h"    // usb-ethernet drivers
       #include "lwip/init.h"               // initialize lwIP stack
       #include "lwip/timeouts.h"           // lwIP cyclic timers
       #include "lwip/netif.h"              // network interfaces

2. **Stack Initialization**: Users will need to perform an extremely complicated procedure to initialize the lwip stack.

       lwip_init();

3. **Initialize the USB-Ethernet Driver**: The procedure for initializing the USB Ethernet driver is equally complicated.

       if (usb_Init(eth_handle_usb_event, NULL, NULL, USB_DEFAULT_INIT_FLAGS))
           goto exit;    // this can be any hard-exit condition

3. **Poll for an Active Netif**: The usb-Ethernet driver is fully-internalized and does not expose anything, but does register a netif when a new Ethernet device is detected. To detect an active netif, you can poll in your own code for the presence of a netif. You may use only the first netif to handle your application's network or you can map netifs to different connections. The only determining factor is how big of a migraine you want.

       struct netif* ethif = NULL;
       do {
           ethif = netif_find("en0");    // this will likely be the first netif returned, probably just use it
       } (ethif == NULL);
       // ethif is now the netif you will bind when using networks in your application
              

5. **Create a Do-While-Network-Up Loop**: The general flow of your application can be run in a do-while loop with the exit condition being the netif marked as up.

       do {
           // code in here to handle your application
           // keypress detection, rendering graphics, etc
           usb_HandleEvents();     // polls/triggers all USB events - allows Ethernet drivers to function
           sys_check_timeouts();   // polls/triggers all lwIP timer events/callbacks - allows IP stack to function
       } while(netif_is_up(ethif));

6. **Use the Appropriate API for your Application from the lwIP Docs**: As I value my dwindling sanity, I will not be re-documenting the entire lwIP codebase in here. The documentation for lwIP is here: https://www.nongnu.org/lwip/2_1_x/group__callbackstyle__api.html. The callback-style "raw" API is the only thing that will work in a NOSYS implementation. Do not implement the threaded API and then wonder why it does not work on a device that doesn't know what the heck a thread is.

   If you require assistance with the API for lwIP, feel free to ask in the Discord: https://discord.gg/kvcuygqU



