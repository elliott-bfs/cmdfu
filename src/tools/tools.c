#include <string.h>
#include <errno.h>
#include <assert.h>
#include "mdfu/tools/tools.h"
#include "mdfu/tools/network.h"
#include "mdfu/tools/spidev.h"
#include "mdfu/tools/i2cdev.h"
#include "mdfu/tools/serial.h"
#include "mdfu/logging.h"
/**
 * @brief Array of tool names.
 *
 * This array contains the names of the tools available.
 * Each entry corresponds to a tool, and the array is terminated with a NULL pointer.
 *
 * @note The order of the names in this array must match the order of the tools
 *       in the `tools` array and in the tool_type_t enum.
 */
const char *tool_names[] = {"serial", "network", "spidev", "i2cdev", NULL};

/**
 * @brief Array of tool pointers.
 *
 * This array contains pointers to the tool_t structures corresponding to the tools
 * available. Each entry corresponds to a tool, and the array is terminated
 * with a NULL pointer.
 *
 * @note The order of the tools in this array should match the order of the names
 *       in the `tool_names` array and in the tool_type_t enum.
 */
static tool_t *tools[] = {&serial_tool, &network_tool, &spidev_tool, &i2cdev_tool, NULL};

/**
 * @brief Retrieves the name of a tool based on its type.
 *
 * This function takes a tool type and returns the corresponding tool name
 * through the provided pointer. It performs checks to ensure the tool type
 * is valid and the name pointer is not NULL.
 *
 * @param tool The type of the tool (of type `tool_type_t`).
 * @param name A pointer to a const char pointer where the tool name will be stored.
 *             This pointer must not be NULL.
 * 
 * @return 0 on success.
 * @return -1 if the tool type is invalid.
 */
int get_tool_name_by_type(tool_type_t tool, const char **name){
    if (name == NULL) {
        return -1; // Error: Null pointer passed for name
    }
    if(tool < 0 || tool >= TOOL_NONE){
        return -1;
    }
    *name = tool_names[tool];
    return 0;
}

/**
 * @brief Retrieves a tool by its name.
 *
 * This function searches for a tool in the `tool_names` array and, if found,
 * sets the corresponding tool in the `tool` pointer provided by the caller.
 *
 * @param[in] name The name of the tool to search for. This parameter should not be modified.
 * @param[out] tool A pointer to a tool_t pointer where the found tool will be stored.
 *                  If the tool is found, *tool will point to the corresponding tool_t object.
 *                  If the tool is not found, *tool will not be modified.
 *
 * @return 0 if the tool is found and *tool is set to the corresponding tool_t object.
 * @return -1 if the tool is not found.
 */
int get_tool_by_name(const char *name, tool_t **tool){
    // Make sure that we have the same number of tool names as we have tools
    assert(sizeof(tool_names) / sizeof(char *) == sizeof(tools) / sizeof(tool_t *));

    if(name == NULL || tool == NULL){
        return -1;
    }

    for(int i = 0; tool_names[i] != NULL; i++){
        if(0 == strcmp(name, tool_names[i])){
            *tool = tools[i];
            return 0;
        }
    }
    return -1;
}

/**
 * @brief Retrieves a tool by its type.
 *
 * This function retrieves a tool based on the specified tool type and assigns it to the provided
 * tool pointer. It performs a range check on the tool type and ensures that the tool type is valid.
 *
 * @param[in] tool_type The type of the tool to retrieve. This should be a value of type `tool_type_t`.
 * @param[out] tool A pointer to a `tool_t*` where the retrieved tool will be stored.
 *
 * @return 0 on success, -1 on failure.
 *
 * @retval 0 The tool was successfully retrieved and assigned to the provided pointer.
 * @retval -1 The tool type is invalid, and an error message is logged.
 */
int get_tool_by_type(tool_type_t tool_type, tool_t **tool){
    assert((TOOL_NONE + 1) == sizeof(tools) / sizeof(tool_t *));
    if(tool_type >= TOOL_NONE || tool_type < 0){
        ERROR("Invalid tool type");
        return -1;
    }
    *tool = tools[tool_type];
    return 0;
}