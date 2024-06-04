#ifndef NETWORK_H
#define NETWORK_H
#include "mdfu/tools.h"
#include "mdfu/socket_mac.h"

struct network_config {
    struct socket_config socket_config;
};

extern tool_t network_tool;
#endif