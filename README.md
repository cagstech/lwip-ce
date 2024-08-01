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

**Use the Appropriate API for your Application from the lwIP Docs**: As I value my dwindling sanity, I will not be re-documenting the entire lwIP codebase in here. The documentation for callback-style API is here: https://www.nongnu.org/lwip/2_1_x/group__callbackstyle__api.html. If you require assistance with the API for lwIP, feel free to ask in the [Discord](https://discord.gg/kvcuygqU) or contact the lwIP authors directly using the link above.

Your imagination (and I guess the remaining heap space) on the calculator are your limits. You can use lwIP in literally the same way that you would on a computer or any other device. The only constraint is that you are limited to the *callback-style API*. You cannot use modules such as sockets, netconn and others that require an OS. That doesn't matter much though, as you can use the slightly-lower-level raw API for protocols like TCP, UDP, ICMP, and more. Here is an example of a simple TCP-client application that sets up a network interface, connects to a TCP server, then disconnects. *Note that it does not wait for FIN when disconnecting and in a proper TCP client you must.*

        #include "lwip/netif.h"     // we will use the netif_find function
        #include "lwip/tcp.h"       // raw TCP API
    
        err_t do_when_connected(void *arg, struct tcp_pcb *tpcb, err_t err);
    
        int main(void) {
            // .. the init logic ..
    
            bool do_main_loop = true;
            struct netif* ethif = NULL;
            struct tcp_pcb *pcb = tcp_new();
            do {
                // code in here to handle your application
                // keypress detection, rendering graphics, etc
                if(!ethif){
                    ethif = netif_find("en0");
                    if(ethif){
                        netif_set_default(ethif);   // anything we do defaults to this IF now
                        ip_addr_t remote = IPADDR4_INIT_BYTES(<ip to connect to>);
                        if(tcp_connect(pcb, &remote, <port>, do_when_connected) != ERR_OK)
                            do_error_handle();
                    }
                }
                usb_HandleEvents();
                sys_check_timeouts();
            } while(do_main_loop);
        }
    
        // tcp connected callback
        err_t do_when_connected(void *arg, struct tcp_pcb *tpcb, err_t err){
            if(err == ERR_OK){
                printf("connection successful, disconnecting...");
                if(tcp_close(pcb) != ERR_OK)
                    printf("disconnect error");
            }
            else
                printf("connect error");
        }
    
        // this is just the tip of the iceberg of how to implement TCP and designed to give
        // you a feel for how to set it up, not a comprehensive TCP client.


**Proper Cleanup and Exit**

Networked applications are not simple programs where you can just exit with no consequences. An average use of lwIP has timers in effect, callbacks, queued data transfers, packet buffers, control blocks -- a lot of memory-occupying allocated resources. If you just exit without properly setting down these resources... you can imagine the results. You should ideally follow a proper sequence of deinitializing and removing resources.

1. Call the appropriate close() functions on any protocol control blocks. For TCP, await a FIN response from the remote using the callbacks. Wait for connections to PROPERLY close, which causes lwIP to release their resources.
    
2. Once all PCBs are confirmed closed, begin to deregister network interfaces. Call `netif_remove()` on any interface that is active.
    
3. Call usb_Cleanup() to immediately terminate all pending USB transfers, reset device(s), and free resources associated with the USB driver.
    
4. NOW you can exit your application.
