#include <string.h>
#include <errno.h>
#include <assert.h>
#include "mdfu/tools/tools.h"
#include "mdfu/tools/network.h"
#include "mdfu/tools/spidev.h"
#include "mdfu/tools/serial.h"
#include "mdfu/logging.h"

const char *tool_names[] = {"serial", "network", "spidev", NULL};
static tool_t *tools[] = {&serial_tool, &network_tool, &spidev_tool, NULL};

int get_tool_name_by_type(tool_type_t tool, const char **name){
    if(tool >= TOOL_NONE){
        return -1;
    }
    *name = tool_names[tool];
    return 0;
}

int get_tool_by_name(char *name, tool_t **tool){
    assert(sizeof(tool_names) / sizeof(char *) == sizeof(tools) / sizeof(tool_t *));

    for(int i = 0; tool_names[i] != NULL; i++){
        if(0 == strcmp(name, tool_names[i])){
            *tool = tools[i];
            return 0;
        }
    }
    return -1;
}

int get_tool_by_type(tool_type_t tool_type, tool_t **tool){
    assert((TOOL_NONE + 1) == sizeof(tools) / sizeof(tool_t *));
    if(tool_type >= TOOL_NONE){
        ERROR("Invalid tool type");
        return -1;
    }
    *tool = tools[tool_type];
    return 0;
}