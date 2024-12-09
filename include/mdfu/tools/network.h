#ifndef NETWORK_H
#define NETWORK_H
#include "mdfu/tools/tools.h"
#include "mdfu/mac/socket_mac.h"
#include "mdfu/transport/transport.h"

struct network_config {
    struct socket_config socket_config;
    transport_type_t transport;
};

extern tool_t network_tool;
#endif