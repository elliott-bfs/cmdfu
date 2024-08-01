#pragma once
#include "mdfu/tools.h"
#include "mdfu/serial_mac.h"

struct serial_config {
    int baudrate,
    char *port
};

extern tool_t serial_tool;