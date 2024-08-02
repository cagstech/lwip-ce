# lwIP-CE #

![AUTOBUILD](https://github.com/cagstech/lwip-ce/actions/workflows/build.yml/badge.svg)

## What are lwIP and lwIP-CE ##

**lwIP** is a full networking stack for low-resource device like embedded systems. This makes it perfect for something as ridiclous as a graphing calculator.
It is maintained by non-GNU (https://github.com/lwip-tcpip/lwip).
**lwIP-CE** is the name for the lwIP fork targetting the Texas Instruments TI-84+ CE graphing calculator.
You can view the original readme [here](./README-ORIG.md).

This implementation differs from the ported lwIP in the following ways:
- reduced pbuf pool and tcp_sndbuf queue size
- non "raw" API's will not work due to NOSYS implementation
- hardware-specific USB CDC-ECM and NCM drivers

## Related Media ##
1. https://www.youtube.com/watch?v=fD2n7CzFeZU


# lwIP Stack Initialization #

Programs using lwIP as a dynamic library need to follow a specific initialization sequence to start up the stack and the link-layer. It is **extremely** important that you do things in the order shown to ensure that various callbacks and timers initialize in the correct order.

1. **Include the Necessary Headers**: The following headers are needed for things to work at all.

        #include <usbdrvce.h>                   // USB driver
        #include "drivers/usb-ethernet.h"       // CDC-Ethernet driver (ECM/NCM)
        #include "lwip/init.h"                  // lwIP initialization
        // NOTE: if you use additional modules, you will need those headers as well
    
    
2. **Configure lwIP to Use the Program's Malloc Implementation**: This is something you cannot skip. In order to allocate memory and not conflict with your own program, lwIP needs to use your program's implementation of malloc. This is so important that I actually modified the lwIP source to return an error on init if this is not done. *Note: The max_heap (third) argument to that function sets a limit on how much heap space lwIP is allowed to use. This is useful for tailoring memory constraints to your use case. If your program needs more for itself, configure lwIP to use less. If you anticipate sending lots of data with lwIP, set it higher.*

        #define LWIP_MAX_HEAP   (1024 * 16)
        lwip_configure_allocator(malloc, free, LWIP_MAX_HEAP);


3. **Initialize the lwIP Stack**: Finally we fire up the IP stack.

        if(lwip_init() != ERR_OK)
            goto exit;      // whatever your exit w/ error method is
        
        
4. **Initialize the CDC-Ethernet Driver**: `eth_handle_usb_event` is the entry point to the data-link layer driver for Ethernet provided in this library. Initialize the calculator's USB hardware, passing that function as a callback as shown below.

        if (usb_Init(eth_handle_usb_event, NULL, NULL, USB_DEFAULT_INIT_FLAGS))
            goto exit;      // whatever your exit w/ error method is      

        
# Using the lwIP API # 

## Callback-Style API ##

I'll be direct. lwIP is not a trivial thing to use. As the TI-84+ CE does not possess what qualifies as an operating system for the purposes of lwIP, we are restricted to the use of the raw API, also called the *callback API*. In this framework you declare a resource for a connection, called a *protocol control block (PCB)* and you register callback functions to the PCB for various actions that may occur on that resource -- sent, recvd, connected, error, etc. lwIP handles the routing of those packets and processing of network events on the PCBs, executing the callbacks in response to appropriate event. This means you will need familiarity with callback/event-driven programming to use lwIP.

he documentation for callback-style API is here: https://www.nongnu.org/lwip/2_1_x/group__callbackstyle__api.html. As you will see if you spend any amount of time perusing the documentation you will find that in many places it tells you next to nothing about what functions do. If you require assistance with the API for lwIP, feel free to ask in the [Discord](https://discord.gg/kvcuygqU) or contact the lwIP authors directly using the link above. 

## Error Handling ##

To be fully stable your application needs to properly handle any errors that may arise. This section describes error handling already implemented at various parts of the network model:

- **data link**: USB CDC Ethernet drivers implement retries for failed bulk transfers; reaching retry max disables the device which in turn disables lwIP to prevent continued operation on a damaged driver state.

You also need to handle responses to network events -- 
handle error conditions. The remote host may return an error. The connection may fail and then reconnect, a packet may fail to send because of queue full or memory issues. Your application needs to be robust enough to handle that and your use case will determine whether your response is just to lose the packet or to queue it for retry later.

T


# Proper Cleanup and Exit #


Networked applications 
