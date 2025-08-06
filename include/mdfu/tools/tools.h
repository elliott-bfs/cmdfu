#ifndef TOOLS_H
#define TOOLS_H

#include "mdfu/transport/transport.h"

typedef struct {
    transport_t ops;
    int (* init)(void *config);
    void (* list_connected_tools)(void);
    int (* parse_arguments)(int tool_argc, char **tool_argv, void **config);
    char *(* get_parameter_help)(void);
}tool_t;

typedef enum tool_type {
#ifdef USE_TOOL_SERIAL
    TOOL_SERIAL,
#endif
#ifdef USE_TOOL_NETWORK
    TOOL_NETWORK,
#endif
#ifdef USE_TOOL_SPI
    TOOL_SPIDEV,
#endif
#ifdef USE_TOOL_I2C
    TOOL_I2CDEV,
#endif
    TOOL_NONE
}tool_type_t;

extern const char *tool_names[];

int get_tool_by_name(const char *name, tool_t **tool);
int get_tool_by_type(tool_type_t tool_type, tool_t **tool);
int get_tool_name_by_type(tool_type_t tool, const char **name);
#endif