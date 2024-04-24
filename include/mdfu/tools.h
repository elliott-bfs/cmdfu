#pragma once
#include "transport.h"

typedef struct {
    int (* init)(void *config);
    int (* open)(void);
    int (* close)(void);
    int (* read)(int *, uint8_t *);
    int (* write)(int, uint8_t *);
    void (* list_connected_tools)(void);
    int (* parse_arguments)(int tool_argc, char **tool_argv, void **config);
}tool_t;

typedef enum tool_type {
    TOOL_SERIAL = 0,
    TOOL_MCP2221A = 1,
    TOOL_NETWORK = 2,
    TOOL_NONE = 3
}tool_type_t;

int get_tool_by_name(char *name, tool_t **tool);
int get_tool_by_type(tool_type_t tool_type, tool_t **tool);