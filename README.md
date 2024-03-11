This is an implementation of a TCP/IP stack for the Texas Instruments TI-84+ CE graphing calculator.
It it derived/ported from https://github.com/lwip-tcpip/lwip.
To view the original README, open README-ORIG.md.

This implementation has reduced pbufs, queue lengths due to memory constraints, and several non-essential/unsupported modules disabled.
Implementation is NOSYS but event-based.

Link-layer drivers are: 
- CDC Ethernet Control Model (ECM)

Related Media:
- https://www.youtube.com/watch?v=fD2n7CzFeZU
