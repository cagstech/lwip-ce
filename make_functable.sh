#!/bin/bash

# Explicitly include headers
declare -a include_headers=(
    # ethernet driver
    "drivers/usb-ethernet.h"
    "include/lwip/etharp.h"
    "include/lwip/ethip6.h"
    # stats checking
    "include/lwip/igmp.h"
    "include/lwip/snmp.h"
    "include/lwip/stats.h"
    # ip stuff
    "include/lwip/ip4.h"
    "include/lwip/ip4_addr.h"
    "include/lwip/ip4_frag.h"
    "include/lwip/ip6.h"
    "include/lwip/ip6_addr.h"
    "include/lwip/ip6_frag.h"
    "include/lwip/ip6_zone.h"
    "include/lwip/ip.h"
    "include/lwip/ip_addr.h"
    "include/lwip/autoip.h"
    # buffers and mem
    "include/lwip/pbuf.h"
    "include/lwip/mem.h"
    "include/lwip/memp.h"
    # lwip general
    "include/lwip/init.h"
    "include/lwip/netif.h"
    "include/lwip/dhcp.h"
    "include/lwip/dns.h"
    "include/lwip/nd6.h"
    "include/lwip/raw.h"
    "include/lwip/tcp.h"
    "include/lwip/altcp.h"
    "include/lwip/altcp_tcp.h"
    "include/lwip/tcpbase.h"
    "include/lwip/udp.h"
)

# Remove any prototypes disabled by macros to avoid errors
declare -a excluded_functions=(
    "etharp_add_static_entry"
    "etharp_remove_static_entry"
    "mib2_add_arp_entry"
    "mib2_add_ip4"
    "mib2_add_route_ip4"
    "mib2_netif_added"
    "mib2_netif_removed"
    "mib2_remove_arp_entry"
    "mib2_remove_ip4"
    "mib2_remove_route_ip4"
    "mib2_udp_bind"
    "mib2_udp_unbind"
    "ip4_debug_print"
    "ip4_output_hinted"
    "ip4_route_src"
    "ip6_debug_print"
    "ip6_output_hinted"
    "pbuf_fill_chksum"
    "pbuf_split_64k"
    "memp_malloc_fn"
    "netif_get_loopif"
    "dhcp_set_ntp_servers"
    "dns_local_addhost"
    "dns_local_iterate"
    "dns_local_lookup"
    "dns_local_removehost"
    "TCP_PCB_COMMON"
    "lwip_tcp_event"
    "tcp_ext_arg_alloc_id"
    "tcp_ext_arg_get"
    "tcp_ext_arg_set"
    "tcp_ext_arg_set_callbacks"
    "altcp_keepalive_disable"
    "altcp_keepalive_enable"
    "udp_debug_print"
    "udp_send_chksum"
    "udp_sendto_chksum"
    "udp_sendto_if_chksum"
    "udp_sendto_if_src_chksum"
)

# output_functable keeps a running list of functions we have already put into the table
# the order of functions of the functable order cannot change for forward compatibility.
output_functable=src/functable.inc
# functable_c is the C code used to reserve the functiontable in the application
output_c=src/functable.c


[[ ! -f $output_functable ]] && touch $output_functable

# Function to process files
process_file() {
    local file="src/$1"     # access needs relative to this dir
    echo "Processing file: $file"
    /usr/local/bin/ctags -f- --kinds-C=p $file | awk '{ print $1; }' | sed 's/$/,/' | while read -r line; do
        skip=0
        if ! grep -Fxq "$line" "$output_functable"; then
            for exclude in "${excluded_functions[@]}"; do
                if [[ "$exclude," =~ $line ]]; then
                    echo "found $line in excluded_headers, skipping"
                    skip=1
                    break
                fi
            done
            if [ $skip -eq 0 ]; then
                echo "appending function: $line"
                echo "$line" >> "$output_functable"
            fi
        fi
    done
}


# Logic
rm $output_c
for file in "${include_headers[@]}"; do
    # Find all C header files specified
        process_file "$file"
        echo "#include \"$file\"" >> $output_c
done
echo "" >> $output_c
echo "" >> $output_c
echo "void *functable[] __attribute__((section(\"thunks\"))) = {" >> $output_c
cat $output_functable >> $output_c
echo "};" >> $output_c

