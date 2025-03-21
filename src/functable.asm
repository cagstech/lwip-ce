section .rodata
public _app_library_table

_app_library_table:
    dl _eth_get_interfaces
    dl _eth_usb_event_callback
    dl _etharp_acd_announce
    dl _etharp_acd_probe
    dl _etharp_cleanup_netif
    dl _etharp_find_addr
    dl _etharp_get_entry
    dl _etharp_input
    dl _etharp_output
    dl _etharp_query
    dl _etharp_request
    dl _etharp_tmr
    dl _ethip6_output
    dl _igmp_init
    dl _igmp_input
    dl _igmp_joingroup
    dl _igmp_joingroup_netif
    dl _igmp_leavegroup
    dl _igmp_leavegroup_netif
    dl _igmp_lookfor_group
    dl _igmp_report_groups
    dl _igmp_start
    dl _igmp_stop
    dl _igmp_tmr
    dl _stats_display
    dl _stats_display_igmp
    dl _stats_display_mem
    dl _stats_display_memp
    dl _stats_display_proto
    dl _stats_display_sys
    dl _stats_init
    dl _ip4_input
    dl _ip4_output
    dl _ip4_output_if
    dl _ip4_output_if_opt
    dl _ip4_output_if_opt_src
    dl _ip4_output_if_src
    dl _ip4_route
    dl _ip4_set_default_multicast_netif
    dl _ip4_addr_isbroadcast_u32
    dl _ip4_addr_netmask_valid
    dl _ip4addr_aton
    dl _ip4addr_ntoa
    dl _ip4addr_ntoa_r
    dl _ipaddr_addr
    dl _ip4_frag
    dl _ip4_reass
    dl _ip_reass_tmr
    dl _ip6_input
    dl _ip6_options_add_hbh_ra
    dl _ip6_output
    dl _ip6_output_if
    dl _ip6_output_if_src
    dl _ip6_route
    dl _ip6_select_source_address
    dl _ip6addr_aton
    dl _ip6addr_ntoa
    dl _ip6addr_ntoa_r
    dl _ip6_frag
    dl _ip6_reass
    dl _ip6_reass_tmr
    dl _ip_input
    dl _ipaddr_aton
    dl _ipaddr_ntoa
    dl _ipaddr_ntoa_r
    dl _autoip_accept_packet
    dl _autoip_network_changed_link_down
    dl _autoip_network_changed_link_up
    dl _autoip_remove_struct
    dl _autoip_set_struct
    dl _autoip_start
    dl _autoip_stop
    dl _autoip_supplied_address
    dl _pbuf_add_header
    dl _pbuf_add_header_force
    dl _pbuf_alloc
    dl _pbuf_alloc_reference
    dl _pbuf_alloced_custom
    dl _pbuf_cat
    dl _pbuf_chain
    dl _pbuf_clen
    dl _pbuf_clone
    dl _pbuf_coalesce
    dl _pbuf_copy
    dl _pbuf_copy_partial
    dl _pbuf_copy_partial_pbuf
    dl _pbuf_dechain
    dl _pbuf_free
    dl _pbuf_free_header
    dl _pbuf_free_ooseq
    dl _pbuf_get_at
    dl _pbuf_get_contiguous
    dl _pbuf_header
    dl _pbuf_header_force
    dl _pbuf_memcmp
    dl _pbuf_memfind
    dl _pbuf_put_at
    dl _pbuf_realloc
    dl _pbuf_ref
    dl _pbuf_remove_header
    dl _pbuf_skip
    dl _pbuf_strstr
    dl _pbuf_take
    dl _pbuf_take_at
    dl _pbuf_try_get_at
    dl _custom_calloc
    dl _custom_free
    dl _custom_malloc
    dl _mem_calloc
    dl _mem_free
    dl _mem_init
    dl _mem_malloc
    dl _mem_trim
    dl _memp_free
    dl _memp_init
    dl _memp_malloc
    dl _lwip_init
    dl _netif_add
    dl _netif_add_ext_callback
    dl _netif_add_ip6_address
    dl _netif_add_noaddr
    dl _netif_alloc_client_data_id
    dl _netif_create_ip6_linklocal_address
    dl _netif_find
    dl _netif_get_by_index
    dl _netif_get_ip6_addr_match
    dl _netif_index_to_name
    dl _netif_init
    dl _netif_input
    dl _netif_invoke_ext_callback
    dl _netif_ip6_addr_set
    dl _netif_ip6_addr_set_parts
    dl _netif_ip6_addr_set_state
    dl _netif_loop_output
    dl _netif_name_to_index
    dl _netif_poll
    dl _netif_poll_all
    dl _netif_remove
    dl _netif_remove_ext_callback
    dl _netif_set_addr
    dl _netif_set_default
    dl _netif_set_down
    dl _netif_set_gw
    dl _netif_set_ipaddr
    dl _netif_set_link_callback
    dl _netif_set_link_down
    dl _netif_set_link_up
    dl _netif_set_netmask
    dl _netif_set_remove_callback
    dl _netif_set_status_callback
    dl _netif_set_up
    dl _dhcp_cleanup
    dl _dhcp_coarse_tmr
    dl _dhcp_fine_tmr
    dl _dhcp_inform
    dl _dhcp_network_changed_link_up
    dl _dhcp_release
    dl _dhcp_release_and_stop
    dl _dhcp_renew
    dl _dhcp_set_struct
    dl _dhcp_start
    dl _dhcp_stop
    dl _dhcp_supplied_address
    dl _dns_gethostbyname
    dl _dns_gethostbyname_addrtype
    dl _dns_getserver
    dl _dns_init
    dl _dns_setserver
    dl _dns_tmr
    dl _nd6_adjust_mld_membership
    dl _nd6_cleanup_netif
    dl _nd6_clear_destination_cache
    dl _nd6_find_route
    dl _nd6_get_destination_mtu
    dl _nd6_get_next_hop_addr_or_queue
    dl _nd6_input
    dl _nd6_reachability_hint
    dl _nd6_restart_netif
    dl _nd6_tmr
    dl _raw_bind
    dl _raw_bind_netif
    dl _raw_connect
    dl _raw_disconnect
    dl _raw_new
    dl _raw_new_ip_type
    dl _raw_recv
    dl _raw_remove
    dl _raw_send
    dl _raw_sendto
    dl _raw_sendto_if_src
    dl _tcp_abort
    dl _tcp_accept
    dl _tcp_arg
    dl _tcp_backlog_accepted
    dl _tcp_backlog_delayed
    dl _tcp_bind
    dl _tcp_bind_netif
    dl _tcp_close
    dl _tcp_connect
    dl _tcp_err
    dl _tcp_listen_with_backlog
    dl _tcp_listen_with_backlog_and_err
    dl _tcp_new
    dl _tcp_new_ip_type
    dl _tcp_output
    dl _tcp_poll
    dl _tcp_recv
    dl _tcp_recved
    dl _tcp_sent
    dl _tcp_setprio
    dl _tcp_shutdown
    dl _tcp_tcp_get_tcp_addrinfo
    dl _tcp_write
    dl _altcp_abort
    dl _altcp_accept
    dl _altcp_arg
    dl _altcp_bind
    dl _altcp_close
    dl _altcp_connect
    dl _altcp_dbg_get_tcp_state
    dl _altcp_err
    dl _altcp_get_ip
    dl _altcp_get_port
    dl _altcp_get_tcp_addrinfo
    dl _altcp_listen_with_backlog_and_err
    dl _altcp_mss
    dl _altcp_nagle_disable
    dl _altcp_nagle_disabled
    dl _altcp_nagle_enable
    dl _altcp_new
    dl _altcp_new_ip6
    dl _altcp_new_ip_type
    dl _altcp_output
    dl _altcp_poll
    dl _altcp_recv
    dl _altcp_recved
    dl _altcp_sent
    dl _altcp_setprio
    dl _altcp_shutdown
    dl _altcp_sndbuf
    dl _altcp_sndqueuelen
    dl _altcp_write
    dl _altcp_tcp_alloc
    dl _altcp_tcp_new_ip_type
    dl _altcp_tcp_wrap
    dl _tcp_debug_state_str
    dl _udp_bind
    dl _udp_bind_netif
    dl _udp_connect
    dl _udp_disconnect
    dl _udp_init
    dl _udp_input
    dl _udp_netif_ip_addr_changed
    dl _udp_new
    dl _udp_new_ip_type
    dl _udp_recv
    dl _udp_remove
    dl _udp_send
    dl _udp_sendto
    dl _udp_sendto_if
    dl _udp_sendto_if_src


extern _eth_get_interfaces
extern _eth_usb_event_callback
extern _etharp_acd_announce
extern _etharp_acd_probe
extern _etharp_cleanup_netif
extern _etharp_find_addr
extern _etharp_get_entry
extern _etharp_input
extern _etharp_output
extern _etharp_query
extern _etharp_request
extern _etharp_tmr
extern _ethip6_output
extern _igmp_init
extern _igmp_input
extern _igmp_joingroup
extern _igmp_joingroup_netif
extern _igmp_leavegroup
extern _igmp_leavegroup_netif
extern _igmp_lookfor_group
extern _igmp_report_groups
extern _igmp_start
extern _igmp_stop
extern _igmp_tmr
extern _stats_display
extern _stats_display_igmp
extern _stats_display_mem
extern _stats_display_memp
extern _stats_display_proto
extern _stats_display_sys
extern _stats_init
extern _ip4_input
extern _ip4_output
extern _ip4_output_if
extern _ip4_output_if_opt
extern _ip4_output_if_opt_src
extern _ip4_output_if_src
extern _ip4_route
extern _ip4_set_default_multicast_netif
extern _ip4_addr_isbroadcast_u32
extern _ip4_addr_netmask_valid
extern _ip4addr_aton
extern _ip4addr_ntoa
extern _ip4addr_ntoa_r
extern _ipaddr_addr
extern _ip4_frag
extern _ip4_reass
extern _ip_reass_tmr
extern _ip6_input
extern _ip6_options_add_hbh_ra
extern _ip6_output
extern _ip6_output_if
extern _ip6_output_if_src
extern _ip6_route
extern _ip6_select_source_address
extern _ip6addr_aton
extern _ip6addr_ntoa
extern _ip6addr_ntoa_r
extern _ip6_frag
extern _ip6_reass
extern _ip6_reass_tmr
extern _ip_input
extern _ipaddr_aton
extern _ipaddr_ntoa
extern _ipaddr_ntoa_r
extern _autoip_accept_packet
extern _autoip_network_changed_link_down
extern _autoip_network_changed_link_up
extern _autoip_remove_struct
extern _autoip_set_struct
extern _autoip_start
extern _autoip_stop
extern _autoip_supplied_address
extern _pbuf_add_header
extern _pbuf_add_header_force
extern _pbuf_alloc
extern _pbuf_alloc_reference
extern _pbuf_alloced_custom
extern _pbuf_cat
extern _pbuf_chain
extern _pbuf_clen
extern _pbuf_clone
extern _pbuf_coalesce
extern _pbuf_copy
extern _pbuf_copy_partial
extern _pbuf_copy_partial_pbuf
extern _pbuf_dechain
extern _pbuf_free
extern _pbuf_free_header
extern _pbuf_free_ooseq
extern _pbuf_get_at
extern _pbuf_get_contiguous
extern _pbuf_header
extern _pbuf_header_force
extern _pbuf_memcmp
extern _pbuf_memfind
extern _pbuf_put_at
extern _pbuf_realloc
extern _pbuf_ref
extern _pbuf_remove_header
extern _pbuf_skip
extern _pbuf_strstr
extern _pbuf_take
extern _pbuf_take_at
extern _pbuf_try_get_at
extern _custom_calloc
extern _custom_free
extern _custom_malloc
extern _mem_calloc
extern _mem_configure
extern _mem_free
extern _mem_init
extern _mem_malloc
extern _mem_trim
extern _memp_free
extern _memp_init
extern _memp_malloc
extern _lwip_init
extern _netif_add
extern _netif_add_ext_callback
extern _netif_add_ip6_address
extern _netif_add_noaddr
extern _netif_alloc_client_data_id
extern _netif_create_ip6_linklocal_address
extern _netif_find
extern _netif_get_by_index
extern _netif_get_ip6_addr_match
extern _netif_index_to_name
extern _netif_init
extern _netif_input
extern _netif_invoke_ext_callback
extern _netif_ip6_addr_set
extern _netif_ip6_addr_set_parts
extern _netif_ip6_addr_set_state
extern _netif_loop_output
extern _netif_name_to_index
extern _netif_poll
extern _netif_poll_all
extern _netif_remove
extern _netif_remove_ext_callback
extern _netif_set_addr
extern _netif_set_default
extern _netif_set_down
extern _netif_set_gw
extern _netif_set_ipaddr
extern _netif_set_link_callback
extern _netif_set_link_down
extern _netif_set_link_up
extern _netif_set_netmask
extern _netif_set_remove_callback
extern _netif_set_status_callback
extern _netif_set_up
extern _dhcp_cleanup
extern _dhcp_coarse_tmr
extern _dhcp_fine_tmr
extern _dhcp_inform
extern _dhcp_network_changed_link_up
extern _dhcp_release
extern _dhcp_release_and_stop
extern _dhcp_renew
extern _dhcp_set_struct
extern _dhcp_start
extern _dhcp_stop
extern _dhcp_supplied_address
extern _dns_gethostbyname
extern _dns_gethostbyname_addrtype
extern _dns_getserver
extern _dns_init
extern _dns_setserver
extern _dns_tmr
extern _nd6_adjust_mld_membership
extern _nd6_cleanup_netif
extern _nd6_clear_destination_cache
extern _nd6_find_route
extern _nd6_get_destination_mtu
extern _nd6_get_next_hop_addr_or_queue
extern _nd6_input
extern _nd6_reachability_hint
extern _nd6_restart_netif
extern _nd6_tmr
extern _raw_bind
extern _raw_bind_netif
extern _raw_connect
extern _raw_disconnect
extern _raw_new
extern _raw_new_ip_type
extern _raw_recv
extern _raw_remove
extern _raw_send
extern _raw_sendto
extern _raw_sendto_if_src
extern _tcp_abort
extern _tcp_accept
extern _tcp_arg
extern _tcp_backlog_accepted
extern _tcp_backlog_delayed
extern _tcp_bind
extern _tcp_bind_netif
extern _tcp_close
extern _tcp_connect
extern _tcp_err
extern _tcp_listen_with_backlog
extern _tcp_listen_with_backlog_and_err
extern _tcp_new
extern _tcp_new_ip_type
extern _tcp_output
extern _tcp_poll
extern _tcp_recv
extern _tcp_recved
extern _tcp_sent
extern _tcp_setprio
extern _tcp_shutdown
extern _tcp_tcp_get_tcp_addrinfo
extern _tcp_write
extern _altcp_abort
extern _altcp_accept
extern _altcp_arg
extern _altcp_bind
extern _altcp_close
extern _altcp_connect
extern _altcp_dbg_get_tcp_state
extern _altcp_err
extern _altcp_get_ip
extern _altcp_get_port
extern _altcp_get_tcp_addrinfo
extern _altcp_listen_with_backlog_and_err
extern _altcp_mss
extern _altcp_nagle_disable
extern _altcp_nagle_disabled
extern _altcp_nagle_enable
extern _altcp_new
extern _altcp_new_ip6
extern _altcp_new_ip_type
extern _altcp_output
extern _altcp_poll
extern _altcp_recv
extern _altcp_recved
extern _altcp_sent
extern _altcp_setprio
extern _altcp_shutdown
extern _altcp_sndbuf
extern _altcp_sndqueuelen
extern _altcp_write
extern _altcp_tcp_alloc
extern _altcp_tcp_new_ip_type
extern _altcp_tcp_wrap
extern _tcp_debug_state_str
extern _udp_bind
extern _udp_bind_netif
extern _udp_connect
extern _udp_disconnect
extern _udp_init
extern _udp_input
extern _udp_netif_ip_addr_changed
extern _udp_new
extern _udp_new_ip_type
extern _udp_recv
extern _udp_remove
extern _udp_send
extern _udp_sendto
extern _udp_sendto_if
extern _udp_sendto_if_src
