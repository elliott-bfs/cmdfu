#ifndef TOOLS_H
#define TOOLS_H

#include "mdfu/transport.h"

typedef struct {
    transport_t ops;
    int (* init)(void *config);
    void (* list_connected_tools)(void);
    int (* parse_arguments)(int tool_argc, char **tool_argv, void **config);
    char *(* get_parameter_help)(void);
}tool_t;

typedef enum tool_type {
    TOOL_SERIAL = 0,
    TOOL_MCP2221A = 1,
    TOOL_NETWORK = 2,
    TOOL_NONE = 3
}tool_type_t;

int get_tool_by_name(char *name, tool_t **tool);
int get_tool_by_type(tool_type_t tool_type, tool_t **tool);
#endif