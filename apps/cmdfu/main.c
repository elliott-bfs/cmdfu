#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <getopt.h>
#include "version.h"
#include "mdfu/logging.h"
#include "mdfu/mdfu.h"
#include "cmdfu.h"

struct args args = {
    .help = false,
    .version = false,
    .tool = TOOL_NONE,
    .action = ACTION_NONE,
    .image = NULL
};
extern int parse_mdfu_update_arguments(int argc, char **argv, int *new_argc, char **new_argv);
extern int parse_common_arguments(int argc, char **argv, int *action_argc, char **action_argv);

/**
 * @brief Retrieves and prints MDFU client information.
 *
 * This function performs the following steps:
 * 1. Retrieves the tool based on the provided type.
 * 2. Parses the arguments for the tool.
 * 3. Initializes the tool.
 * 4. Initializes the MDFU protocol.
 * 5. Opens a connection to the tool.
 * 6. Retrieves client information.
 * 7. Prints the client information.
 * 8. Closes the connection and frees allocated resources.
 *
 * If any step fails, an error message is printed, resources are freed, and the function exits with a failure code.
 *
 * @param argc The number of tool arguments.
 * @param argv The tool argument vector.
 * @return 0 on success, -1 on failure.
 */
static int mdfu_client_info(int argc, char **argv){
    tool_t *tool;
    void *tool_conf = NULL;
    client_info_t client_info;

    if(get_tool_by_type(args.tool, &tool) < 0){
        ERROR("Invalid tool selected");
        goto err_exit;
    }
    if(tool->parse_arguments(argc, argv, &tool_conf) < 0){
        ERROR("Invalid tool argument");
        goto err_exit;
    }
    if(tool->init(tool_conf) < 0){
        ERROR("Tool initialization failed");
        goto err_exit;
    }

    if(mdfu_init(&tool->ops, 2) < 0){
        ERROR("MDFU protocol initialization failed");
        goto err_exit;
    }

    if(mdfu_open() < 0){
        ERROR("Connecting to tool failed");
        goto err_exit;
    }

    if(mdfu_get_client_info(&client_info) < 0){
        ERROR("Failed to get client info");
        mdfu_close();
        goto err_exit;
    }
    print_client_info(&client_info);
    mdfu_close();
    free(tool_conf);
    return 0;

    err_exit:
        if(NULL != tool_conf){
            free(tool_conf);
        }
        return -1;
}

/**
 * @brief Perform a firmware update using the specified tool and image file.
 *
 * This function handles the process of updating firmware by performing the following steps:
 * 1. Retrieve the tool based on the specified type.
 * 2. Parse the arguments for the tool configuration.
 * 3. Open the firmware image file.
 * 4. Initialize the tool with the parsed configuration.
 * 5. Initialize the MDFU protocol.
 * 6. Connect to the tool.
 * 7. Run the firmware update process.
 * 8. Close the MDFU connection and the firmware image file reader.
 *
 * @param argc The number of tool arguments.
 * @param argv The array of tool arguments.
 * @return 0 on success, -1 on failure.
 */
static int mdfu_update(int argc, char **argv){
    tool_t *tool;
    void *tool_conf = NULL;

    if(get_tool_by_type(args.tool, &tool) < 0){
        ERROR("Invalid tool selected");
        goto err_exit;
    }
    if(tool->parse_arguments(argc, argv, &tool_conf) < 0){
        ERROR("Invalid tool argument");
        goto err_exit;
    }
    if(fwimg_file_reader.open(args.image) < 0){
        ERROR("Opening image file failed: %s", strerror(errno));
        goto err_exit;
    }
    if(tool->init(tool_conf) < 0){
        ERROR("Tool initialization failed");
        goto err_exit;
    }

    if(mdfu_init(&tool->ops, 2) < 0){
        ERROR("MDFU protocol initialization failed");
        goto err_exit;
    }

    if(mdfu_open() < 0){
        ERROR("Connecting to tool failed");
        goto err_exit;
    }

    if(mdfu_run_update(&fwimg_file_reader) < 0){
        ERROR("Firmware update failed");
        goto err_exit;
    }
    mdfu_close();
    fwimg_file_reader.close();
    printf("Firmware update completed successfully\n");
    return 0;

    err_exit:
        fwimg_file_reader.close();
        mdfu_close();
        if(NULL != tool_conf){
            free(tool_conf);
        }
        return -1;
}

/**
 * @brief Perform a firmware dump (download) using the specified tool and output file.
 *
 * This function handles the process of dumping firmware by performing the following steps:
 * 1. Retrieve the tool based on the specified type.
 * 2. Parse the arguments for the tool configuration.
 * 3. Open the output file for writing the dumped firmware.
 * 4. Initialize the tool with the parsed configuration.
 * 5. Initialize the MDFU protocol.
 * 6. Connect to the tool.
 * 7. Run the firmware dump process.
 * 8. Close the MDFU connection and the output file writer.
 *
 * @param argc The number of tool arguments.
 * @param argv The array of tool arguments.
 * @return 0 on success, -1 on failure.
 */
static int mdfu_dump(int argc, char **argv){
    tool_t *tool;
    void *tool_conf = NULL;

    if(get_tool_by_type(args.tool, &tool) < 0){
        ERROR("Invalid tool selected");
        goto err_exit;
    }
    if(tool->parse_arguments(argc, argv, &tool_conf) < 0){
        ERROR("Invalid tool argument");
        goto err_exit;
    }
    if(fwimg_file_writer.open(args.image) < 0){
        ERROR("Opening output file failed: %s", strerror(errno));
        goto err_exit;
    }
    if(tool->init(tool_conf) < 0){
        ERROR("Tool initialization failed");
        goto err_exit;
    }

    if(mdfu_init(&tool->ops, 2) < 0){
        ERROR("MDFU protocol initialization failed");
        goto err_exit;
    }

    if(mdfu_open() < 0){
        ERROR("Connecting to tool failed");
        goto err_exit;
    }

    if(mdfu_run_dump(&fwimg_file_writer) < 0){
        ERROR("Firmware dump failed");
        goto err_exit;
    }
    mdfu_close();
    fwimg_file_writer.close();
    printf("Firmware dump completed successfully\n");
    return 0;

    err_exit:
        fwimg_file_writer.close();
        mdfu_close();
        if(NULL != tool_conf){
            free(tool_conf);
        }
        return -1;
}

/**
 * @brief Perform a mode change using the specified tool.
 *
 * This function handles the process of changing mode by performing the
 * following steps:
 * 1. Retrieve the tool based on the specified type.
 * 2. Parse the arguments for the tool configuration.
 * 3. Initialize the tool with the parsed configuration.
 * 4. Initialize the MDFU protocol.
 * 5. Connect to the tool.
 * 6. Run the change mode process.
 * 7. Close the MDFU connection and the firmware image file reader.
 *
 * @param argc The number of tool arguments.
 * @param argv The array of tool arguments.
 * @return 0 on success, -1 on failure.
 */
static int mdfu_change_mode(int argc, char **argv) {
  tool_t *tool;
  void *tool_conf = NULL;

  if (get_tool_by_type(args.tool, &tool) < 0) {
    ERROR("Invalid tool selected");
    goto err_exit;
  }
  if (tool->parse_arguments(argc, argv, &tool_conf) < 0) {
    ERROR("Invalid tool argument");
    goto err_exit;
  }
  if (tool->init(tool_conf) < 0) {
    ERROR("Tool initialization failed");
    goto err_exit;
  }

  if (mdfu_init(&tool->ops, 2) < 0) {
    ERROR("MDFU protocol initialization failed");
    goto err_exit;
  }

  if (mdfu_open() < 0) {
    ERROR("Connecting to tool failed");
    goto err_exit;
  }

  if (mdfu_run_change_mode() < 0) {
    ERROR("Change mode failed");
    goto err_exit;
  }
  mdfu_close();
  printf("Mode change completed successfully\n");
  return 0;

err_exit:
  mdfu_close();
  if (NULL != tool_conf) {
    free(tool_conf);
  }
  return -1;
}

/**
 * @brief Displays help information for all available tools.
 * @brief Performs firmware update using the specified tool configuration.
 * This function iterates through all the tool names and retrieves the corresponding tool
 * structure. If the tool has a help function for its parameters, it calls this function
 * and prints the returned help text to the standard output.
 *
 * @note The function assumes that `tool_names` is a NULL-terminated array of strings,
 *       where each string is the name of a tool.
 *
 * @note The function `get_tool_by_name` is used to retrieve the tool structure based on
 *       the tool name. It is assumed that this function returns 0 on success.
 *
 * @note The function `tool->get_parameter_help` is assumed to return a string containing
 *       the help text for the tool's parameters.
 */
void tools_help(void){
    tool_t *tool;
    char *cli_help_text;
    int status;

    for(const char **tool_name = tool_names; *tool_name != NULL; tool_name++){
        status = get_tool_by_name(*tool_name, &tool);
        if(0 == status && tool->get_parameter_help != NULL){
            cli_help_text = tool->get_parameter_help();
            printf("%s", cli_help_text);
        }
    }
}

/**
 * @brief Main entry point for MDFU application
 * 
 * @param argc Command line arguments count
 * @param argv Command line arguments list
 * @return int Status
 */
int main(int argc, char **argv)
{
    int exit_status = 0;
    // Keep it simple and allocate enough space for pointers to all
    // arguments in argv since we don't know how many of the options
    // are tools options.
    char **action_argv = malloc(argc * sizeof(void *));
    int action_argc;
    char **tool_argv;
    int tool_argc;

    init_logging(stderr);

    exit_status = parse_common_arguments(argc, argv, &action_argc, action_argv);

    if(0 == exit_status){
        switch(args.action){
            case ACTION_UPDATE:
                tool_argv = malloc(action_argc * sizeof(void *));
                exit_status = parse_mdfu_update_arguments(action_argc, action_argv, &tool_argc, tool_argv);
                if(0 == exit_status){
                    exit_status = mdfu_update(tool_argc, tool_argv);
                }
                break;
            case ACTION_CLIENT_INFO:
                exit_status = mdfu_client_info(action_argc, action_argv);
                break;
            case ACTION_TOOLS_HELP:
                tools_help();
                break;
            case ACTION_CHANGE_MODE:
                exit_status = mdfu_change_mode(action_argc, action_argv);
                break;
            case ACTION_DUMP:
                tool_argv = malloc(action_argc * sizeof(void *));
                exit_status = parse_mdfu_update_arguments(action_argc, action_argv, &tool_argc, tool_argv);
                if(0 == exit_status){
                    exit_status = mdfu_dump(tool_argc, tool_argv);
                }
                break;
            default:
                break;
        }
    }
    exit(exit_status);
}
