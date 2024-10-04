#ifndef NETWORK_H
#define NETWORK_H
#include "mdfu/tools/tools.h"
#include "mdfu/mac/socket_mac.h"

#define NET_TOOL_TRANSPORT_SERIAL 0
#define NET_TOOL_TRANSPORT_SPI 1

struct network_config {
    struct socket_config socket_config;
    int transport;
};

extern tool_t network_tool;
#endif