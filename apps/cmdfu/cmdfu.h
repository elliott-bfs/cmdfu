#ifndef CMDFU_H
#define CMDFU_H

#include <stdbool.h>
#include "mdfu/tools/tools.h"

/**
 * @enum action_t
 * @brief Enumeration for different types of actions.
 *
 * This enumeration defines the possible actions that can be taken.
 */
typedef enum {
  ACTION_UPDATE = 0,
  ACTION_CLIENT_INFO = 1,
  ACTION_TOOLS_HELP = 2,
  ACTION_CHANGE_MODE = 3,
  ACTION_NONE = 4
} action_t;

/**
 * struct args - Structure to hold data from parsed command line arguments.
 * @help: Boolean flag to indicate if help information is requested.
 * @version: Boolean flag to indicate if version information is requested.
 * @tool: Enum to specify the tool type.
 * @action: Enum to specify the action to be performed.
 * @image: Pointer to a character array holding the update firmware image file name or path.
 */
struct args {
    bool help;
    bool version;
    tool_type_t tool;
    action_t action;
    char * image;
};

extern struct args args;

#endif