#pragma once
#include "transport.h"

typedef struct {
    int (* get_transport)(transport_t *);
    void (* list_supported_tools)(void);
}tool_t;

int get_tool(char *name, tool_t *tool);