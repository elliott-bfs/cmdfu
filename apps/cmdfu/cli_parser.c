#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <getopt.h>
#include "version.h"
#include "mdfu/logging.h"
#include "mdfu/mdfu_config.h"
#include "cmdfu.h"

static const char *actions[] = {"update", "client-info", "tools-help", NULL};

static const char *help_usage = "cmdfu [-h | --help] [-v <level> | --verbose <level>] [-V | --version] [-R | --release-info] [<action>]";
static const char *help_update = "cmdfu [--help | -h] [--verbose <level> | -v <level>] [--config-file <file> | -c <file>] "
    "update --tool <tool> --image <image> [<tools-args>...]";
static const char *help_client_info = "cmdfu [--help | -h] [--verbose <level> | -v <level>] [--config-file <file> | -c <file>] "
    "client-info --tool <tool> [<tools-args>...]";
static const char *help_tools = "cmdfu [--help | -h] [--verbose <level> | -v <level>] tools-help";
static const char *help_common =
    "Actions\n"
    "    <action>        Action to perform. Valid actions are:\n"
    "    client-info:    Get MDFU client information\n"
    "    tools-help:     Get help on tool specific parameters\n"
    "    update:         Perform a firmware update\n"
    "\n"
    "    -h, --help      Show this help message and exit\n"
    "\n"
    "    -V, --version   Print cmdfu version number and exit\n"
    "\n"
    "    -R, --release-info\n"
    "                    Print cmdfu release details and exit\n"
    "\n"
    "Optional arguments\n"
    "    -v <level>, --verbose <level>\n"
    "                    Logging verbosity/severity level. Valid levels are\n"
    "                    [debug, info, warning, error, critical].\n"
    "                    Default is info.\n"
    "\n"
    "Usage examples\n"
    "\n"
    "    Update firmware through serial port and with update_image.img\n"
    "    cmdfu update --tool serial --image update_image.img --port COM11 --baudrate 115200\n";



/**
 * @brief Prints a formatted message followed by a concatenated list of options.
 *
 * This function takes a message and an array of strings (options), concatenates
 * all the options into a single string separated by spaces, and then prints
 * the message followed by the concatenated options string using the DEBUG macro.
 *
 * @param message The message to be printed before the options.
 * @param argv An array of strings representing the options. The array must be
 *             NULL-terminated.
 */
static void print_options(const char *message, char **argv){

    int size = 0;
    if(NULL != argv && *argv != NULL){
        // Sum up the number of characters for all options and spaces
        for (char **ptr = argv; *ptr != NULL; ptr++) {
            size += strlen(*ptr) + 1; // +1 for the space or null terminator
        }
        // Auto Variable Length Array (VLA)
        // Optional in C11 so might be better with alloca or go for heap with malloc
        char buf[size];
        char *pbuf = buf;
        for (char **ptr = argv; *ptr != NULL; ptr++){
            pbuf = stpcpy(pbuf, *ptr);
            *pbuf = ' ';
            pbuf++;
        }
        *(pbuf - 1) = '\0'; // Replace the last space with a null terminator
        DEBUG("%s %s", message, buf);
    }
}

/**
 * @brief Get logging verbosity level by name
 *
 * @param level_name Logging verbosity level name string
 * @return int, -1 for invalid name, positive values representing valid levels
 */
static int get_log_level_by_name(const char *level_name){
    char *debug_levels[] = {"error", "warning", "info", "debug", NULL};
    int level = -1;

    for(int i = 0; NULL != debug_levels[i]; i++){
        if(0 == strcmp(debug_levels[i], level_name)){
            level = i + 1;
            break;
        }
    }
    return level;
}

/**
 * @brief Print help text for CLI actions.
 *
 * @param action Defines the action for which the help text should be printed
 */
static void print_help_for_action(action_t action){
    if(action == ACTION_NONE){
        printf("%s\n", help_usage);
        printf("%s\n", help_common);
    } else if(args.action == ACTION_UPDATE){
        printf("%s\n", help_update);
    } else if(args.action == ACTION_CLIENT_INFO){
        printf("%s\n", help_client_info);
    } else if(args.action == ACTION_TOOLS_HELP){
        printf("%s\n", help_tools);
    }

}

/**
 * handle_verbose_option - Sets the verbosity level based on the provided level name.
 * @level_name: A string representing the desired verbosity level.
 *
 * This function retrieves the log level corresponding to the provided level name.
 * If the level name is invalid, it logs an error message. Otherwise, it sets the
 * debug level to the retrieved log level and logs a debug message indicating the
 * new verbosity level.
 */
static void handle_verbose_option(char *level_name) {
    int lvl = get_log_level_by_name(level_name);
    if (lvl == -1) {
        printf("Invalid verbosity level - %s\n", level_name);
    } else {
        set_debug_level(lvl);
        printf("Verbosity set to %s\n", level_name);
    }
}

/**
 * @brief Handle the --tool option argument.
 *
 * This function checks if the provided tool name is valid by comparing it against
 * a list of known tool names. If the tool name is valid, it sets the tool type
 * in the global args structure. If the tool name is invalid, it prints an error
 * message and lists the valid tool names.
 *
 * @param tool_name The name of the tool provided as an argument.
 * @return true if the tool name is valid, false otherwise.
 */
static bool handle_tool_option(char *tool_name) {
    bool tool_found = false;

    // Check if --tool argument is valid
    for(const char **p = tool_names; *p != NULL; p++){
        if(strcmp(tool_name, *p) == 0){
            args.tool = (tool_type_t) (p - tool_names);
            tool_found = true;
            break;
        }
    }
    // If tool argument is invalid print error message
    if(!tool_found){
        printf("Unkown tool \"%s\" for --tool option argument\n", tool_name);
        printf("Valid tools are: ");
        for(const char **p = tool_names; *p != NULL; p++){
            printf("%s ", *p);
        }
        printf("\n");
        tool_found = false;
    }
    return tool_found;
}

/**
 * Handle an unrecognized option from the command line arguments.
 *
 * This function saves the unrecognized option in the tools options list and
 * checks if the next item is an option or an option value. If it is an option
 * value, it adds it to the tools arguments list and skips the next argument
 * in getopt_long.
 *
 * @param argv The array of command line arguments.
 * @param action_argc A pointer to the count of action arguments.
 * @param action_argv The array to store action arguments.
 */
static void handle_unrecognized_option(char **argv, int *action_argc, char **action_argv) {
    // Save the unrecognized option in tools options list
    action_argv[*action_argc] = argv[optind - 1];
    (*action_argc)++;
    // check if next item is an option or an option value
    // if it is an option value add it to the tools arguments list
    if (argv[optind] != NULL && argv[optind][0] != '-') {
        action_argv[*action_argc] = argv[optind];
        (*action_argc)++;
        // skip next argument in getopt_long
        optind++;
    }
}

/**
 * Handle the action argument provided in the command line arguments.
 *
 * This function processes the action argument from the command line arguments
 * and sets the appropriate action in the global `args` structure. If no action
 * is provided, or if too many actions are provided, or if an unknown action is
 * provided, it handles these cases by printing appropriate messages and setting
 * the `args.action` to `ACTION_NONE`.
 *
 * @param argc The number of command line arguments.
 * @param argv The array of command line arguments.
 * @param print_help A pointer to a boolean that will be set to true if help
 *                   should be printed.
 */
static void handle_action_argument(int argc, char **argv, bool *print_help) {
    if (optind >= argc) {
        printf("No action provided. Valid actions are: ");
        for (const char **p = actions; *p != NULL; p++) {
            printf("%s ", *p);
        }
        puts("");
        *print_help = true;
        return;
    }

    if (argc - optind > 1) {
        printf("Too many actions provided\n");
        args.action = ACTION_NONE;
        return;
    }

    for (const char **p = actions; *p != NULL; p++) {
        if (strcmp(argv[optind], *p) == 0) {
            args.action = (action_t)(p - actions);
            return;
        }
    }

    printf("Unknown action \"%s\"\n", argv[optind]);
    printf("Valid actions are: ");
    for (const char **p = actions; *p != NULL; p++) {
        printf("%s ", *p);
    }
    puts("");
    args.action = ACTION_NONE;
}

/**
 * @brief Parses common command-line arguments and handles specific options.
 *
 * This function processes command-line arguments using getopt_long to handle
 * various options such as verbose, version, release, help, and tool. It also
 * manages unrecognized options and missing arguments. The function prepares
 * the remaining arguments for further parsing by other parsers.
 *
 * @param argc The argument count.
 * @param argv The argument vector.
 * @param action_argc Pointer to an integer to store the count of action arguments.
 * @param action_argv Array to store the action arguments.
 * @return Returns 0 on success, -1 on error.
 */
int parse_common_arguments(int argc, char **argv, int *action_argc, char **action_argv){
    struct option long_options[] =
    {
        {"verbose", required_argument, NULL, 'v'},
        {"version", no_argument, 0, 'V'},
        {"release", no_argument, 0, 'R'},
        {"help", no_argument, 0, 'h'},
        {"tool", required_argument, NULL, 't'},
        // last entry must be implemented with name as zero
        {0, 0, 0, 0}
    };
    int opt;
    bool error_exit = false;
    bool _exit = false;
    bool print_help = false;
    bool end_of_options = false;

    // Disable error logging in getopt for not recognized options because
    // we will handle these in separate parsers.
    opterr = 0;

    // For parsing the tools arguments with getopt_long we need to have a
    // dummy argument at the beginning since the re-initialization of the
    // parsing library will set optind to 1
    action_argv[0] = "action args";
    *action_argc = 1;

    while (!error_exit && !end_of_options)
    {
        /* getopt_long stores the option index here. */
        int option_index = 0;

        // The leading colon on the options string instructs getopt_long
        // to return a : instead of ? when a missing argument is encountered
        opt = getopt_long(argc, argv, ":v:hVRt:",
                        long_options, &option_index);

        /* Detect the end of the options. */
        if (opt == -1){
            end_of_options = true;
            continue;
        }

        switch (opt)
        {
        case 'v':
            handle_verbose_option(optarg);
            break;
        case 'V':
            printf("Version: %d.%d.%d\n", CMDFU_VERSION_MAJOR, CMDFU_VERSION_MINOR, CMDFU_VERSION_PATCH);
            _exit = true;
            break;
        case 'R':
            printf("cmdfu version: %d.%d.%d\n", CMDFU_VERSION_MAJOR, CMDFU_VERSION_MINOR, CMDFU_VERSION_PATCH);
            printf("MDFU protocol version: %s\n", MDFU_PROTOCOL_VERSION);
            _exit = true;
        case 'h':
            // We need to defer the help until we have parsed the actions
            // so that we can show specific help for an action
            print_help = true;
            break;
        case 't':
            if(!handle_tool_option(optarg)){
                error_exit = true;
                _exit = true;
            }
            break;
        case '?':
            // At this point usually an error message would have been printed
            // but we suppressed this by setting opterr to 0
            handle_unrecognized_option(argv, action_argc, action_argv);
            break;
        case ':':
            // Handle missing argument error
            printf("Error: Option %s is missing its argument\n", argv[optind -1]);
            _exit = true;
            error_exit = true;
            break;
        default:
            printf("Did not recognize this argument\n");
            _exit = true;
            error_exit = true;
        }
    }
    if(!_exit){
        // Remaining item in argv must be an action
        handle_action_argument(argc, argv, &print_help);
        if(print_help){
            print_help_for_action(args.action);
            args.action = ACTION_NONE;
            _exit = true;
        }
    }

    if(!_exit){
        // Add remaining arguments into next parser
        for(char **p = &argv[++optind]; *p != NULL; p++){
            action_argv[*action_argc] = *p;
            *action_argc += 1;
        }

        // Make sure last pointer in tool_argv is a NULL pointer to indicate end of array
        action_argv[*action_argc] = NULL;

        if(DEBUGLEVEL == debug_level){
            print_options("Tool arguments after initial parsing:", action_argv);
        }
    }
    if(error_exit){
        return -1;
    }
    return 0;
}

/**
 * @brief Parse update action CLI options
 *
 * Parse update action options and return unrecognized options.
 *
 * @param argc Argument count for parsing
 * @param argv Argument vector for parsing
 * @param new_argc Pointer for storing the number of unrecognized options
 * @param new_argv Pointer to array of pointers that will contain references to
 *                 the unrecognized options
 * @return 0 for success, -1 for error
 */
int parse_mdfu_update_arguments(int argc, char **argv, int *new_argc, char **new_argv){
    struct option long_options[] =
    {
        {"image", required_argument, NULL, 'i'},
        // last entry must be implemented with name as zero
        {0, 0, 0, 0}
    };

    int opt;
    bool error_exit = false;
    bool end_of_options = false;
    // Setting optind to zero triggers a re-initialization of the getopt parsing
    // library. This also sets the optind to the default value of 1 after the
    // initialization, thus we have a dummy value as first element in the array.
    optind = 0;
    // Disable error logging in getopt for not recognized options because
    // we will handle these in separate parsers.
    opterr = 0;

    // For parsing the tools arguments with getopt_long we need to have a
    // dummy argument at the beginning since the re-initialization of the
    // parsing library will set optind to 1
    new_argv[0] = "update args";
    *new_argc = 1;
    
    while (!error_exit && !end_of_options)
    {
        /* getopt_long stores the option index here. */
        int option_index = 0;

        // The leading colon on the options string instructs getopt_long
        // to return a : instead of ? when a missing argument is encountered
        opt = getopt_long(argc, argv, ":i:",
                        long_options, &option_index);

        /* Detect the end of the options. */
        if (opt == -1){
            end_of_options = true;
        }else{
            switch(opt){
            case 'i':
                args.image = optarg;
                break;

            case '?':
                // At this point usually an error message would have been printed
                // but we suppressed this by setting opterr to 0
                handle_unrecognized_option(argv, new_argc, new_argv);
                break;

            default:
                printf("Invalid argument\n");
                error_exit = true;
                break;
            }
        }
    }
    if(!error_exit && args.tool == TOOL_NONE){
        printf("Missing required --tool option\n");
        error_exit = true;
    }
    if(!error_exit && (NULL == args.image)){
        printf("Missing required --image option\n");
        error_exit = true;
    }
    return error_exit ? -1 : 0;
}
