#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <getopt.h>
#include "version.h"
#include "mdfu/logging.h"
#include "mdfu/tools/tools.h"
#include "mdfu/mdfu.h"

typedef enum {
    ACTION_UPDATE = 0,
    ACTION_CLIENT_INFO = 1,
    ACTION_TOOLS_HELP = 2,
    ACTION_NONE = 3
}action_t;

const char *actions[] = {"update", "client-info", "tools-help", NULL};

struct args {
    bool help;
    bool version;
    tool_type_t tool;
    action_t action;
    char * image;
};

struct args args = {
    .help = false,
    .version = false,
    .tool = TOOL_NONE,
    .action = ACTION_NONE,
    .image = NULL
};

const char *help_usage = "cmdfu [-h | --help] [-v <level> | --verbose <level>] [-V | --version] [-R | --release-info] [<action>]";
const char *help_update = "cmdfu [--help | -h] [--verbose <level> | -v <level>] [--config-file <file> | -c <file>] "
    "update --tool <tool> --image <image> [<tools-args>...]";
const char *help_client_info = "cmdfu [--help | -h] [--verbose <level> | -v <level>] [--config-file <file> | -c <file>] "
    "client-info --tool <tool> [<tools-args>...]";
const char *help_tools = "cmdfu [--help | -h] [--verbose <level> | -v <level>] tools-help";
const char *help_common = "\
Actions\n\
    <action>        Action to perform. Valid actions are:\n\
                    client-info: Get MDFU client information\n\
                    tools-help:  Get help on tool specific parameters\n\
                    update:      Perform a firmware update\n\
\n\
    -h, --help      Show this help message and exit\n\
\n\
    -V, --version   Print cmdfu version number and exit\n\
\n\
    -R, --release-info\n\
                    Print cmdfu release details and exit\n\
\n\
Optional arguments\n\
    -v <level>, --verbose <level>\n\
            Logging verbosity/severity level. Valid levels are\n\
            [debug, info, warning, error, critical].\n\
            Default is info.\n\
\n\
Usage examples\n\
\n\
    Update firmware through serial port and with update_image.img\n\
    cmdfu update --tool serial --image update_image.img --port COM11 --baudrate 115200\n";



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
    // TODO add configurable number of retries
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
    // TODO add configurable number of retries
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

void tools_help(void){
    tool_t *tool;
    char *cli_help_text;

    for(const char **tool_name = tool_names; *tool_name != NULL; tool_name++){
        if(0 == get_tool_by_name((char *) *tool_name, &tool)){
            if(tool->get_parameter_help != NULL){
                cli_help_text = tool->get_parameter_help();
                printf("%s", cli_help_text);
            }
        }
    }
}

/**
 * @brief Get logging verbosity level by name
 *
 * @param level_name Logging verbosity level name string
 * @return int, -1 for invalid name, positive values representing valid levels
 */
int get_log_level_by_name(char *level_name){
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

void print_options(const char *message, char **argv){

    int size = 0;
    // Sum up the number of characters for all options and spaces
    for (char **ptr = argv; *ptr != NULL; ptr++) {
        size += strlen(*ptr) + 1; // +1 for the space or null terminator
    }
    // Auto Variable Length Array (VLA)
    // Optional in C11 so might be better with alloca or go for heap with malloc
    char buf[size];
    char *pbuf = buf;
    for (char **ptr = &argv[1]; *ptr != NULL; ptr++){
        pbuf = stpcpy(pbuf, *ptr);
        *pbuf = ' ';
        pbuf++;
    }
    *(pbuf - 1) = '\0'; // Replace the last space with a null terminator
    DEBUG("%s %s", message, buf);
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
        {"image", required_argument, NULL, 'f'},
        // last entry must be implemented with name as zero
        {0, 0, 0, 0}
    };

    int opt;
    bool error_exit = false;
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
    
    while (1)
    {
        /* getopt_long stores the option index here. */
        int option_index = 0;

        // The leading colon on the options string instructs getopt_long
        // to return a : instead of ? when a missing argument is encountered
        opt = getopt_long(argc, argv, ":i:",
                        long_options, &option_index);

        /* Detect the end of the options. */
        if (opt == -1)
            break;

        switch (opt)
        {
        case 'f':
            args.image = optarg;
            break;

        case '?':
            // Here we reference all not recognized options and their arguments in
            // the new_argv for later parsing by the tool parser
            new_argv[*new_argc] = argv[optind -1]; 
            *new_argc += 1;
            // check if next item is an option or an option argument
            // if it is an option argument add it to the list of unrecognized options
            if(argv[optind] != NULL){
                if(argv[optind][0] != '-'){ 
                    new_argv[*new_argc] = argv[optind];
                    *new_argc += 1;
                    // skip next argument in getopt_long
                    optind++;
                }
            }
            break;

        default:
            error_exit = true;
            break;
        }
        if(true == error_exit)
        {
            break;
        }
    }
    if(NULL == args.image){
        ERROR("Missing required --image option");
        error_exit = true;
    }
    if(error_exit) return -1;
    return 0;
}

/**
 * @brief Print help text for CLI actions.
 *
 * @param action Defines the action for which the help text should be printed
 */
void print_help_for_action(action_t action){
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
 * @brief Parse common CLI arguments
 * 
 * @param argc Argument count
 * @param argv Argument vector
 * @return int TODO we should return a parser status
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

    // Disable error logging in getopt for not recognized options because
    // we will handle these in separate parsers.
    opterr = 0;

    // For parsing the tools arguments with getopt_long we need to have a
    // dummy argument at the beginning since the re-initialization of the
    // parsing library will set optind to 1
    action_argv[0] = "action args";
    *action_argc = 1;

    while (1)
    {
        /* getopt_long stores the option index here. */
        int option_index = 0;

        // The leading colon on the options string instructs getopt_long
        // to return a : instead of ? when a missing argument is encountered
        opt = getopt_long(argc, argv, ":v:hVRt:",
                        long_options, &option_index);

        /* Detect the end of the options. */
        if (opt == -1)
            break;

        switch (opt)
        {
        case 'v':
            int lvl = get_log_level_by_name(optarg);
            if(-1 == lvl){
                ERROR("Invalid verbosity level - %s\n", optarg);
                error_exit = true;
            } else{
                set_debug_level(lvl);
                DEBUG("Verbosity set to %s", optarg);
            }
            break;
        case 'V':
            printf("Version: %d.%d.%d\n", MDFU_VERSION_MAJOR, MDFU_VERSION_MINOR, MDFU_VERSION_PATCH);
            _exit = true;
            break;
        case 'R':
            printf("cmdfu version: %d.%d.%d\n", MDFU_VERSION_MAJOR, MDFU_VERSION_MINOR, MDFU_VERSION_PATCH);
            printf("MDFU protocol version: %s\n", MDFU_PROTOCOL_VERSION);
            _exit = true;
        case 'h':
            // We need to defer the help until we have parsed the actions
            // so that we can show specific help for an action
            print_help = true;
            break;
        case 't':
            // Check if --tool argument is valid
            for(const char **p = tool_names; *p != NULL; p++){
                if(strcmp(optarg, *p) == 0){
                    args.tool = p - tool_names;
                }
            }
            // If tool argument is invalid print error message
            if(args.tool == TOOL_NONE){
                printf("Unkown tool \"%s\" for --tool option argument\n", optarg);
                printf("Valid tools are: ");
                for(const char **p = tool_names; *p != NULL; p++){
                    printf("%s ", *p);
                }
                printf("\n");
                error_exit = true;
            }
            break;
        case '?':
            // At this point usually an error message would have been printed
            // but we suppressed this by setting opterr to 0 
            //DEBUG("Tool option found %s", argv[optind -1]);
            // Save the unrecognized option in tools options list
            action_argv[*action_argc] = argv[optind - 1];
            *action_argc += 1;
            // check if next item is an option or an option value
            // if it is an option value add it to the tools arguments list
            if(argv[optind] != NULL){
                if(argv[optind][0] != '-'){
                    action_argv[*action_argc] = argv[optind];
                    *action_argc += 1;
                    // skip next argument in getopt_long
                    optind++;
                }
            }
            break;
        case ':':
            printf("Error: Option %s is missing its argument\n", argv[optind -1]);
            _exit = true;
            error_exit = true;
            break;
        default:
            printf("Did not recognize this argument\n");
            _exit = true;
            error_exit = true;
        }
        if(_exit){
            break;
        }
    }
    if(!_exit){
    // Remaining item in argv must be an action
        if(optind < argc)
        {
            if(argc - optind > 1){
                ERROR("Too many actions provided");
                args.action = ACTION_NONE;
            }else{
                for(const char **p = actions; *p != NULL; p++){
                    if(strcmp(argv[optind], *p) == 0){
                        args.action = p - actions;
                        break;
                    }
                }
                if(args.action == ACTION_NONE){
                    printf("Unkown action \"%s\"\n", argv[optind]);
                    printf("Valid actions are: ");
                    for(const char **p = actions; *p != NULL; p++){
                        printf("%s ", *p);
                    }
                    puts("");
                }
            }
        }else{
            print_help = true;
        }
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
            print_options("Tool arguments after initial parsing", action_argv);
        }
    }
    if(_exit && error_exit){
        return -1;
    }
    return 0;
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
            default:
                break;
        }
    }
    exit(exit_status);
}
