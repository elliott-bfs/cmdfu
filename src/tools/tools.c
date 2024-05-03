#include <string.h>
#include <errno.h>
#include "mdfu/tools.h"
#include "mdfu/tools/network.h"
#include "mdfu/logging.h"

int get_tool_by_name(char *name, tool_t **tool){
    if(0 == strcmp(name, "network")){
        *tool = &network_tool;
    } else {
        return -1;
    }
    return 0;
}

int get_tool_by_type(tool_type_t tool_type, tool_t **tool){
    switch(tool_type){
        case TOOL_NETWORK:
            *tool = &network_tool;
            break;

        default:
            ERROR("No tool specified or tool not implemented");
            errno = EINVAL;
            return -EINVAL;
            break;
    }
    return 0;
}