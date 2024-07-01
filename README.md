# lwIP-CE #

![AUTOBUILD](https://github.com/cagstech/lwip-ce/actions/workflows/build.yml/badge.svg)

## What are lwIP and lwIP-CE ##

**lwIP** is a full networking stack for low-resource device like embedded systems. It is maintained by non-GNU (https://github.com/lwip-tcpip/lwip).
This makes it perfect for something as ridiclous as a graphing calculator.
**lwIP-CE** is the name for the lwIP fork targetting the Texas Instruments TI-84+ CE graphing calculator.
You can view the original readme [here](./README-ORIG.md).

This implementation differs from the ported lwIP in the following ways:
- reduced pbuf pool and tcp_sndbuf queue size
- non "raw" API's will not work due to NOSYS implementation
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
           usb_HandleEvents();
           sys_check_timeouts();
       } while(ethif == NULL);
       // ethif is now the netif you will bind when using networks in your application


5. **Create a Do-While-Network-Up Loop**: The general flow of your application can be run in a do-while loop with the exit condition being the netif marked as up.

       do {
           // code in here to handle your application
           // keypress detection, rendering graphics, etc
           usb_HandleEvents();     // polls/triggers all USB events - allows Ethernet drivers to function
           sys_check_timeouts();   // polls/triggers all lwIP timer events/callbacks - allows IP stack to function
       } while(netif_is_up(ethif));

6. **Use the Appropriate API for your Application from the lwIP Docs**: As I value my dwindling sanity, I will not be re-documenting the entire lwIP codebase in here. The documentation for lwIP is here: https://www.nongnu.org/lwip/2_1_x/group__callbackstyle__api.html. The callback-style "raw" API is the only thing that will work in a NOSYS implementation. Do not implement the threaded API and then wonder why it does not work on a device that doesn't know what the heck a thread is. If you require assistance with the API for lwIP, feel free to ask in the [Discord](https://discord.gg/kvcuygqU) or contact the lwIP authors directly using the link above.

7. Always end your code by performing the appropriate cleanup. This means that you call `usb_Cleanup();` at some point between the shutdown of network services and actual end of your program. Make sure you do this on ALL control paths that may lead to terminating the program, as failing to do this may cause USB issues within the OS requiring a reset. Also make sure that you properly close any sockets and protocols you are running. Failing to do so may cause memory leaks. The order is also important here. Remember that with certain network services, they need to cleanly disconnect from remotes and await success messages before the resource is destroyed and the memory is returned to the stack. For example, after calling `tcp_close()` you should ideally await the full TCP shutdown handshake before doing anything else like freeing the network interface or calling `usb_Cleanup()` (if you disable the Ethernet drivers, how are you going to actually receive the disconnect ACKS?). This means pressing the **Clear** key can now no longer just be a `goto exit;`. It should send a `tcp_close()` on any active pcbs, each of which should await their reset/fin/fin-acks before beginning the control flow for ending the program. This should be: (1) close all pcbs and await confirmation, (2) set the interface down (`netif_set_down(yournetif)`), (3) then on `netif_is_down(yournetif)`, `netif_remove(yournetif)`, and then finally (4) `usb_Cleanup()`.
