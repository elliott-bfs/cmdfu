#ifndef SOCKET_MAC_H
#define SOCKET_MAC_H

#include "mac.h"

struct socket_config {
    char * host;
    uint16_t port;
};

void get_socket_mac(mac_t **mac);
void get_socket_packet_mac(mac_t **mac);

#endif