#pragma once
#include "mdfu/tools/tools.h"
#include "mdfu/mac/serial_mac.h"

struct serial_config {
    int baudrate,
    char *port
};

extern tool_t serial_tool;