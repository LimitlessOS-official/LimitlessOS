/* Split scaffold fragment. Real code is unity-included by scaffold.c; direct compilation emits only the anchor below. */

#if defined(LIMITLESS_SCAFFOLD_NETWORK_DRS_AND_N1)
static void log_virtio_net_surface(void)
{
    static const struct scaffold_value_field fields[] = {
        {"found ", SCAFFOLD_VALUE_NET_FOUND, SCAFFOLD_TELEMETRY_DEC},
        {"bar0 ", SCAFFOLD_VALUE_NET_BAR0, SCAFFOLD_TELEMETRY_HEX64},
        {"mapped ", SCAFFOLD_VALUE_NET_MAPPED, SCAFFOLD_TELEMETRY_DEC},
        {"common ", SCAFFOLD_VALUE_NET_COMMON, SCAFFOLD_TELEMETRY_DEC},
        {"notify ", SCAFFOLD_VALUE_NET_NOTIFY, SCAFFOLD_TELEMETRY_DEC},
        {"device-config ", SCAFFOLD_VALUE_NET_DEVICE_CONFIG, SCAFFOLD_TELEMETRY_DEC},
        {"mac ", SCAFFOLD_VALUE_NET_MAC, SCAFFOLD_TELEMETRY_HEX64},
        {"mac-nonzero ", SCAFFOLD_VALUE_NET_MAC_NONZERO, SCAFFOLD_TELEMETRY_DEC},
        {"status-ack ", SCAFFOLD_VALUE_NET_STATUS_ACK, SCAFFOLD_TELEMETRY_DEC},
        {"status-driver ", SCAFFOLD_VALUE_NET_STATUS_DRIVER, SCAFFOLD_TELEMETRY_DEC},
        {"features-ok ", SCAFFOLD_VALUE_NET_FEATURES_OK, SCAFFOLD_TELEMETRY_DEC},
        {"driver-ok ", SCAFFOLD_VALUE_NET_DRIVER_OK, SCAFFOLD_TELEMETRY_DEC},
        {"rx-queue ", SCAFFOLD_VALUE_NET_RX_QUEUE, SCAFFOLD_TELEMETRY_DEC},
        {"tx-queue ", SCAFFOLD_VALUE_NET_TX_QUEUE, SCAFFOLD_TELEMETRY_DEC},
        {"rx-buffers ", SCAFFOLD_VALUE_NET_RX_BUFFERS, SCAFFOLD_TELEMETRY_DEC},
        {"tx ", SCAFFOLD_VALUE_NET_TX, SCAFFOLD_TELEMETRY_DEC},
        {"rx ", SCAFFOLD_VALUE_NET_RX, SCAFFOLD_TELEMETRY_DEC},
        {"arp-reply ", SCAFFOLD_VALUE_NET_ARP_REPLY, SCAFFOLD_TELEMETRY_DEC},
        {"arp-mac ", SCAFFOLD_VALUE_NET_ARP_MAC, SCAFFOLD_TELEMETRY_HEX64},
        {"arp-ip ", SCAFFOLD_VALUE_NET_ARP_IP, SCAFFOLD_TELEMETRY_HEX}
    };
    static const struct scaffold_value_field authority_fields[] = {
        {" fs-authority ", SCAFFOLD_VALUE_NET_FS_AUTHORITY, SCAFFOLD_TELEMETRY_DEC},
        {" storage-authority ", SCAFFOLD_VALUE_NET_STORAGE_AUTHORITY, SCAFFOLD_TELEMETRY_DEC},
        {" ambient-authority ", SCAFFOLD_VALUE_NET_AMBIENT_AUTHORITY, SCAFFOLD_TELEMETRY_DEC},
        {" unavailable ", SCAFFOLD_VALUE_NET_UNAVAILABLE, SCAFFOLD_TELEMETRY_DEC},
        {" error ", SCAFFOLD_VALUE_NET_ERROR, SCAFFOLD_TELEMETRY_DEC}
    };
    write_scaffold_prefixed_value_fields(
        "[x64] drs-net drs-net-",
        " drs-net-",
        fields,
        (u32)(sizeof(fields) / sizeof(fields[0])));
    write_scaffold_value_fields(authority_fields, (u32)(sizeof(authority_fields) / sizeof(authority_fields[0])));
    write_line("");
}

static void log_e1000_surface(void)
{
    static const struct scaffold_value_field fields[] = {
        {"found ", SCAFFOLD_VALUE_E1000_FOUND, SCAFFOLD_TELEMETRY_DEC},
        {"bar0 ", SCAFFOLD_VALUE_E1000_BAR0, SCAFFOLD_TELEMETRY_HEX64},
        {"mapped ", SCAFFOLD_VALUE_E1000_MAPPED, SCAFFOLD_TELEMETRY_DEC},
        {"reset ", SCAFFOLD_VALUE_E1000_RESET, SCAFFOLD_TELEMETRY_DEC},
        {"mac ", SCAFFOLD_VALUE_E1000_MAC, SCAFFOLD_TELEMETRY_HEX64},
        {"mac-nonzero ", SCAFFOLD_VALUE_E1000_MAC_NONZERO, SCAFFOLD_TELEMETRY_DEC},
        {"link-up ", SCAFFOLD_VALUE_E1000_LINK_UP, SCAFFOLD_TELEMETRY_DEC},
        {"rx-queue ", SCAFFOLD_VALUE_E1000_RX_QUEUE, SCAFFOLD_TELEMETRY_DEC},
        {"tx-queue ", SCAFFOLD_VALUE_E1000_TX_QUEUE, SCAFFOLD_TELEMETRY_DEC},
        {"rx-buffers ", SCAFFOLD_VALUE_E1000_RX_BUFFERS, SCAFFOLD_TELEMETRY_DEC},
        {"tx ", SCAFFOLD_VALUE_E1000_TX, SCAFFOLD_TELEMETRY_DEC},
        {"rx ", SCAFFOLD_VALUE_E1000_RX, SCAFFOLD_TELEMETRY_DEC},
        {"dhcp ", SCAFFOLD_VALUE_E1000_DHCP, SCAFFOLD_TELEMETRY_DEC},
        {"dns ", SCAFFOLD_VALUE_E1000_DNS, SCAFFOLD_TELEMETRY_DEC},
        {"http ", SCAFFOLD_VALUE_E1000_HTTP, SCAFFOLD_TELEMETRY_DEC}
    };
    static const struct scaffold_value_field authority_fields[] = {
        {" fs-authority ", SCAFFOLD_VALUE_E1000_FS_AUTHORITY, SCAFFOLD_TELEMETRY_DEC},
        {" storage-authority ", SCAFFOLD_VALUE_E1000_STORAGE_AUTHORITY, SCAFFOLD_TELEMETRY_DEC},
        {" ambient-authority ", SCAFFOLD_VALUE_E1000_AMBIENT_AUTHORITY, SCAFFOLD_TELEMETRY_DEC},
        {" unavailable ", SCAFFOLD_VALUE_E1000_UNAVAILABLE, SCAFFOLD_TELEMETRY_DEC},
        {" error ", SCAFFOLD_VALUE_E1000_ERROR, SCAFFOLD_TELEMETRY_DEC}
    };
    write_scaffold_prefixed_value_fields(
        "[x64] drs-e1000 drs-e1000-",
        " drs-e1000-",
        fields,
        (u32)(sizeof(fields) / sizeof(fields[0])));
    write_scaffold_value_fields(authority_fields, (u32)(sizeof(authority_fields) / sizeof(authority_fields[0])));
    write_line("");
}

static void log_dhcp_surface(void)
{
    static const struct scaffold_value_field fields[] = {
        {"discover ", SCAFFOLD_VALUE_DHCP_DISCOVER, SCAFFOLD_TELEMETRY_DEC},
        {"offer ", SCAFFOLD_VALUE_DHCP_OFFER, SCAFFOLD_TELEMETRY_DEC},
        {"request ", SCAFFOLD_VALUE_DHCP_REQUEST, SCAFFOLD_TELEMETRY_DEC},
        {"ack ", SCAFFOLD_VALUE_DHCP_ACK, SCAFFOLD_TELEMETRY_DEC},
        {"ip ", SCAFFOLD_VALUE_DHCP_IP, SCAFFOLD_TELEMETRY_HEX},
        {"gateway ", SCAFFOLD_VALUE_DHCP_GATEWAY, SCAFFOLD_TELEMETRY_HEX},
        {"dns ", SCAFFOLD_VALUE_DHCP_DNS, SCAFFOLD_TELEMETRY_HEX},
        {"lease ", SCAFFOLD_VALUE_DHCP_LEASE, SCAFFOLD_TELEMETRY_DEC}
    };
    static const struct scaffold_value_field authority_fields[] = {
        {" ambient-authority ", SCAFFOLD_VALUE_DHCP_AMBIENT_AUTHORITY, SCAFFOLD_TELEMETRY_DEC},
        {" unavailable ", SCAFFOLD_VALUE_DHCP_UNAVAILABLE, SCAFFOLD_TELEMETRY_DEC},
        {" error ", SCAFFOLD_VALUE_DHCP_ERROR, SCAFFOLD_TELEMETRY_DEC}
    };
    write_scaffold_prefixed_value_fields(
        "[x64] drs-dhcp drs-dhcp-",
        " drs-dhcp-",
        fields,
        (u32)(sizeof(fields) / sizeof(fields[0])));
    write_scaffold_value_fields(authority_fields, (u32)(sizeof(authority_fields) / sizeof(authority_fields[0])));
    write_line("");
}

static void log_dns_surface(void)
{
    static const struct scaffold_value_field fields[] = {
        {"query ", SCAFFOLD_VALUE_DNS_QUERY, SCAFFOLD_TELEMETRY_DEC},
        {"response ", SCAFFOLD_VALUE_DNS_RESPONSE, SCAFFOLD_TELEMETRY_DEC},
        {"rcode ", SCAFFOLD_VALUE_DNS_RCODE, SCAFFOLD_TELEMETRY_DEC},
        {"resolved ", SCAFFOLD_VALUE_DNS_RESOLVED, SCAFFOLD_TELEMETRY_HEX}
    };
    static const struct scaffold_value_field authority_fields[] = {
        {" fs-authority ", SCAFFOLD_VALUE_DNS_FS_AUTHORITY, SCAFFOLD_TELEMETRY_DEC},
        {" storage-authority ", SCAFFOLD_VALUE_DNS_STORAGE_AUTHORITY, SCAFFOLD_TELEMETRY_DEC},
        {" ambient-authority ", SCAFFOLD_VALUE_DNS_AMBIENT_AUTHORITY, SCAFFOLD_TELEMETRY_DEC},
        {" unavailable ", SCAFFOLD_VALUE_DNS_UNAVAILABLE, SCAFFOLD_TELEMETRY_DEC},
        {" error ", SCAFFOLD_VALUE_DNS_ERROR, SCAFFOLD_TELEMETRY_DEC}
    };
    write_scaffold_prefixed_value_fields(
        "[x64] drs-dns drs-dns-",
        " drs-dns-",
        fields,
        (u32)(sizeof(fields) / sizeof(fields[0])));
    write_scaffold_value_fields(authority_fields, (u32)(sizeof(authority_fields) / sizeof(authority_fields[0])));
    write_line("");
}

static void log_http_surface(void)
{
    static const struct scaffold_value_field fields[] = {
        {"connected ", SCAFFOLD_VALUE_HTTP_CONNECTED, SCAFFOLD_TELEMETRY_DEC},
        {"sent ", SCAFFOLD_VALUE_HTTP_SENT, SCAFFOLD_TELEMETRY_DEC},
        {"status ", SCAFFOLD_VALUE_HTTP_STATUS, SCAFFOLD_TELEMETRY_DEC},
        {"response-bytes ", SCAFFOLD_VALUE_HTTP_RESPONSE_BYTES, SCAFFOLD_TELEMETRY_DEC}
    };
    static const struct scaffold_value_field authority_fields[] = {
        {" fs-authority ", SCAFFOLD_VALUE_HTTP_FS_AUTHORITY, SCAFFOLD_TELEMETRY_DEC},
        {" storage-authority ", SCAFFOLD_VALUE_HTTP_STORAGE_AUTHORITY, SCAFFOLD_TELEMETRY_DEC},
        {" ambient-authority ", SCAFFOLD_VALUE_HTTP_AMBIENT_AUTHORITY, SCAFFOLD_TELEMETRY_DEC},
        {" unavailable ", SCAFFOLD_VALUE_HTTP_UNAVAILABLE, SCAFFOLD_TELEMETRY_DEC},
        {" error ", SCAFFOLD_VALUE_HTTP_ERROR, SCAFFOLD_TELEMETRY_DEC}
    };
    write_scaffold_prefixed_value_fields(
        "[x64] drs-http drs-http-",
        " drs-http-",
        fields,
        (u32)(sizeof(fields) / sizeof(fields[0])));
    write_scaffold_value_fields(authority_fields, (u32)(sizeof(authority_fields) / sizeof(authority_fields[0])));
    write_line("");
}

static void log_network_socket_surface(void)
{
#if defined(LIMITLESS_X64_UEFI_KERNEL) && LIMITLESS_X64_UEFI_KERNEL
    static const u8 net_n1_url[] = "example.com";
    static u8 net_n1_response[4096u];
    u32 net_n1_bytes = 0u;

    network_socket64_probe();
    write_labeled_dec_u32("[x64] drs-socket drs-socket-api ", network_socket64_api_published());
    write_labeled_dec_u32(" drs-socket-service ", network_socket64_service_registered());
    write_labeled_dec_u32(" drs-socket-cap-required ", network_socket64_capability_required());
    write_labeled_dec_u32(" drs-socket-cap-minted ", network_socket64_service_capability_minted());
    write_labeled_dec_u32(" drs-socket-no-cap-denied ", network_socket64_no_cap_denied());
    write_labeled_dec_u32(" drs-socket-wrong-owner-denied ", network_socket64_wrong_owner_denied());
    write_labeled_dec_u32(" drs-socket-raw-denied ", network_socket64_raw_denied());
    write_labeled_dec_u32(" drs-socket-listen-denied ", network_socket64_listen_denied());
    write_labeled_dec_u32(" drs-socket-send-denied ", network_socket64_send_denied());
    write_labeled_dec_u32(" drs-socket-connect-attempt ", network_socket64_connect_attempted());
    write_labeled_dec_u32(" drs-socket-connect-granted ", network_socket64_connect_granted());
    write_labeled_dec_u32(" drs-socket-connect-unavailable ", network_socket64_connect_unavailable());
    write_labeled_dec_u32(" drs-socket-recv-status ", network_socket64_recv_status_granted());
    write_labeled_dec_u32(" drs-socket-close ", network_socket64_close_count());
    write_labeled_dec_u32(" socket-count ", network_socket64_socket_count());
    write_labeled_dec_u32(" http-status ", network_socket64_last_http_status());
    write_labeled_dec_u32(" response-bytes ", network_socket64_last_response_bytes());
    write_labeled_dec_u32(" fs-authority ", network_socket64_fs_authority());
    write_labeled_dec_u32(" storage-authority ", network_socket64_storage_authority());
    write_labeled_dec_u32(" ambient-authority ", network_socket64_ambient_authority());
    write_line("");

    (void)network_socket64_curl_http(
        net_n1_url,
        (u32)(sizeof(net_n1_url) - 1u),
        net_n1_response,
        sizeof(net_n1_response),
        PRINCIPAL64_ID_CONSOLE_CLIENT,
        &net_n1_bytes);
    write_labeled_dec_u32("[x64] net-N1 dns ", network_socket64_curl_dns_resolved());
    write_labeled_dec_u32(" connect ", network_socket64_curl_tcp_connect());
    write_labeled_dec_u32(" http-get ", network_socket64_curl_http_get());
    write_labeled_dec_u32(" response-bytes ", network_socket64_curl_response_bytes());
    write_labeled_dec_u32(" truncated ", network_socket64_curl_truncated());
    write_labeled_dec_u32(" close ", network_socket64_curl_close());
    write_labeled_dec_u32(" cap-minted ", network_socket64_curl_cap_minted());
    write_labeled_dec_u32(" cap-destroyed ", network_socket64_curl_cap_destroyed());
    write_labeled_dec_u32(" nondelegable-denied ", network_socket64_curl_non_delegable_denied());
    write_labeled_dec_u32(" socket-count ", network_socket64_socket_count());
    write_labeled_dec_u32(" fs-authority ", network_socket64_fs_authority());
    write_labeled_dec_u32(" storage-authority ", network_socket64_storage_authority());
    write_labeled_dec_u32(" ambient-authority ", network_socket64_ambient_authority());
    write_labeled_dec_u32(" url-denied ", network_socket64_curl_url_denied());
    write_labeled_dec_u32(" error ", network_socket64_curl_error());
    write_line("");
#endif
}

#endif /* LIMITLESS_SCAFFOLD_NETWORK_DRS_AND_N1 */

#if !defined(LIMITLESS_SCAFFOLD_UNITY)
void limitless_scaffold_network_anchor(void) {}
#endif
