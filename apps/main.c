#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <getopt.h>
#include "version.h"
#include "mdfu/socket_mac.h"
#include "mdfu/serial_transport.h"
#include "mdfu/transport.h"
#include "mdfu/logging.h"
#include "mdfu/tools.h"
#include "mdfu/mdfu.h"

uint8_t get_transfer_parameters_frame[] = {0x56, 0x80, 0x01, 0x7f, 0xfe, 0x9e};
#define GET_TRANSFER_PARAMETERS_RETURN_FRAME_SIZE (4 + 2 + 5 + 2)
uint8_t get_transfer_parameters_mdfu_packet[] = {0x80, 0x01};

typedef enum {
    ACTION_UPDATE = 0,
    ACTION_CLIENT_INFO = 1,
    ACTION_TOOLS_HELP = 2,
    ACTION_NONE = 3
}action_t;

const char *tools[] = {"serial", "mcp2221a", "network", NULL};
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
//char **tool_argv = NULL;
//int tool_argc = 0;
const char *help_usage = "pymdfu [-h | --help] [-v <level> | --verbose <level>] [-V | --version] [-R | --release-info] [<action>]";
const char *help_update = "pymdfu [--help | -h] [--verbose <level> | -v <level>] [--config-file <file> | -c <file>] "
    "update --tool <tool> --image <image> [<tools-args>...]";
const char *help_client_info = "pymdfu [--help | -h] [--verbose <level> | -v <level>] [--config-file <file> | -c <file>] "
    "client-info --tool <tool> [<tools-args>...]";
const char *help_tools = "pymdfu [--help | -h] [--verbose <level> | -v <level>] tools-help";
const char *help_common = "\
Actions\n\
    <action>        Action to perform. Valid actions are:\n\
                    client-info: Get MDFU client information\n\
                    tools-help:  Get help on tool specific parameters\n\
                    update:      Perform a firmware update\n\
\n\
    -h, --help      Show this help message and exit\n\
\n\
    -V, --version   Print pymdfu version number and exit\n\
\n\
    -R, --release-info\n\
                    help=Print pymdfu release details and exit\n\
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
    pymdfu update --tool serial --image update_image.img --port COM11 --baudrate 115200\n";

struct socket_config conf = {
    .host = "192.168.1.20",//"10.146.84.54",
    .port = 5559
};
void test_mac(void){
    mac_t *mac;

    get_socket_mac(&mac);
    mac->init((void *) &conf);
    mac->open();

    mac->write(sizeof(get_transfer_parameters_frame), get_transfer_parameters_frame);
    uint8_t buffer[1024];
    mac->read(GET_TRANSFER_PARAMETERS_RETURN_FRAME_SIZE, buffer);
    
    for(int i=0; i < GET_TRANSFER_PARAMETERS_RETURN_FRAME_SIZE; i++)
    {
        printf("%02x", buffer[i]);
    }
    printf("\n");
    fflush(NULL);
    mac->close();
}

static void test_transport(void){
    mac_t *mac;
    puts("Initializing stack");
    get_socket_mac(&mac);
    if(mac->init((void *) &conf) < 0) {
        puts("Socket MAC init failed");
        return;
    }
    transport_t *transport;
    get_serial_transport(&transport);
    transport->init(mac, 2);

    if(transport->open() < 0){
        return;
    }
    transport->write(sizeof(get_transfer_parameters_mdfu_packet), get_transfer_parameters_mdfu_packet);
    int size = 0;
    uint8_t buffer[1024];
    int status = transport->read(&size, buffer, 1);
    if(status < 0){
        printf("Transport error\n");
        transport->close();
        return;
    }
    transport->close();
}
#if 0
int mdfu_update(transport_t *transport){
    if(transport->open() < 0){
        return -1;
    }
    transport->write(sizeof(get_transfer_parameters_mdfu_packet), get_transfer_parameters_mdfu_packet);
    int size = 0;
    uint8_t buffer[1024];
    int status = transport->read(&size, buffer);
    if(status < 0){
        printf("Transport error\n");
        transport->close();
        return -1;
    }
    transport->close();
}

static int mdfu_client_info(void){
    mac_t *mac;
    transport_t *transport;

    get_socket_mac(&mac);
    if(mac->init((void *) &conf) < 0) {
        DEBUG("Socket MAC init failed");
        return -1;
    }
    get_serial_transport(&transport);
    transport->init(mac, 2);

    mdfu_init(transport, 2);
    mdfu_open();
    client_info_t client_info;
    mdfu_get_client_info(&client_info);
    print_client_info(&client_info);
    mdfu_close();
}
#endif
static int mdfu_client_info(int argc, char **argv){
    tool_t *tool;
    void *tool_conf = NULL;
    client_info_t client_info;

    if(get_tool_by_type(args.tool, &tool) < 0){
        ERROR("Could not get tool");
        return -1;
    }
    
    if(tool->parse_arguments(argc, argv, &tool_conf) < 0){
        ERROR("Tools argument parsing failed");
        return -1;
    }
    if(tool->init(tool_conf) < 0){
        ERROR("Initialization of tool failed");
        return -1;
    }

    if(mdfu_init(&tool->ops, 2) < 0){
        return -1;
    }

    if(mdfu_open() < 0){
        return -1;
    }

    if(mdfu_get_client_info(&client_info) < 0){
        return -1;
    }
    print_client_info(&client_info);
    mdfu_close();
    return 0;
}

static int mdfu_update(int argc, char **argv){
    tool_t *tool;
    void *tool_conf = NULL;
    FILE *image = NULL;
    printf("%s",args.image);
    image = fopen(args.image, "rb");
    if(image == NULL){
        perror("Could not open file:");
        return -1;
    }
    if(get_tool_by_type(args.tool, &tool) < 0){
        perror("Could not get tool");
        return -1;
    }
    
    if(tool->parse_arguments(argc, argv, &tool_conf) < 0){
        perror("Tools argument parsing failed");
        return -1;
    }
    if(tool->init(tool_conf) < 0){
        perror("Initialization of tool failed");
        return -1;
    }
    if(mdfu_init(&tool->ops, 2) < 0){
        return -1;
    }

    if(mdfu_open() < 0){
        return -1;
    }

    if(mdfu_run_update(image) < 0){
        return -1;
    }
    fclose(image);
    return 0;
}

void tools_help(void){
    tool_t *tool;
    char *cli_help_text;

    for(const char **tool_name = tools; *tool_name != NULL; tool_name++){
        if(0 == get_tool_by_name((char *) *tool_name, &tool)){
            if(tool->get_parameter_help != NULL){
                cli_help_text = tool->get_parameter_help();
                printf("%s", cli_help_text);
            }
        }
    }
}

int parse_debug_level(char *level_name){
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
 * @brief Parse common CLI arguments
 * 
 * @param argc Argument count
 * @param argv Argument vector
 * @return int TODO we should return a parser status
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
            printf("Not recognized argument %s, dumping it in argv\n", argv[optind -1]);
            new_argv[*new_argc] = argv[optind -1]; 
            *new_argc += 1;
            // check if next item is an option or an option value
            // if it is an option value add it to the tools arguments list
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
            printf("Did not recognize this argument\n");
            error_exit = true;
            break;
        }
        if(true == error_exit)
        {
            break;
        }
    }
    if(error_exit) return -1;
    return 0;
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
        {"help", no_argument, 0, 'h'},
        {"tool", required_argument, NULL, 't'},
        // last entry must be implemented with name as zero
        {0, 0, 0, 0}
    };
    int opt;
    bool error_exit = false;
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
        opt = getopt_long(argc, argv, ":v:hVt:",
                        long_options, &option_index);

        /* Detect the end of the options. */
        if (opt == -1)
            break;

        switch (opt)
        {
        case 'v':
            int lvl = parse_debug_level(optarg);
            if(-1 == lvl){
                ERROR("Error: invalid verbosity level - %s\n", optarg);
                error_exit = true;
            } else{
                set_debug_level(lvl);
                DEBUG("Verbosity set to %s", optarg);
            }
            break;
        case 'V':
            printf("Version: %d.%d.%d\n", MDFU_VERSION_MAJOR, MDFU_VERSION_MINOR, MDFU_VERSION_PATCH);
            exit(0);
            break;
        case 'h':
            // We need to defer the help until we have parsed the actions
            // so that we can show specific help for an action
            print_help = true;
            break;
        case 't':
            // Check if --tool argument is valid
            for(const char **p = tools; *p != NULL; p++){
                if(strcmp(optarg, *p) == 0){
                    args.tool = p - tools;
                }
            }
            // If tool argument is invalid print error message
            if(args.tool == TOOL_NONE){
                printf("Unkown tool \"%s\" for --tool option argument\n", optarg);
                printf("Valid tools are: ");
                for(const char **p = tools; *p != NULL; p++){
                    printf("%s ", *p);
                }
                printf("\n");
                error_exit = true;
            }
            break;
        case '?':
            // At this point usually an error message would have been printed
            // but we suppressed this by setting opterr to 0 
            // DEBUG("Tool option found %s", argv[optind -1]);
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
            error_exit = true;
            break;
        default:
            printf("Did not recognize this argument\n");
            error_exit = true;
        }
        if(error_exit){
            // TODO proper exit code or implement function return with error code
            exit(1);
        }
    }
#if 0
    printf("options\n");
    for(char **p = argv; *p != NULL; p++){
        printf("%s\n", *p);
    }
    printf("Arguments\n");
    for(char **p = &argv[optind]; *p != NULL; p++){
        printf("%s\n", *p);
    }
#endif
    // Parse non-options
    // Get action argument
    if(optind < argc)
    {
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
    }else{
        print_help = true;
    }
    if(print_help){
        if(args.action == ACTION_NONE){
            printf("%s\n", help_usage);
            printf("%s\n", help_common);
        } else if(args.action == ACTION_UPDATE){
            printf("%s\n", help_update);
        } else if(args.action == ACTION_CLIENT_INFO){
            printf("%s\n", help_client_info);
        } else if(args.action == ACTION_TOOLS_HELP){
            printf("%s\n", help_tools);
        }
        exit(0);
    }
    // Add remaining arguments into next parser
    for(char **p = &argv[++optind]; *p != NULL; p++){
        action_argv[*action_argc++] = *p;
    }
    // Make sure last pointer in tool_argv is a NULL pointer to indicate end of array
    action_argv[*action_argc] = NULL;
    
    // Print second level parser arguments
    // TODO move into function
    int size = 0;
    for (char **ptr = action_argv; *ptr != NULL; ptr++){
        size += strlen(*ptr);
    }
    // Auto Variable Length Array (VLA)
    // Optional in C11 so might be better with alloca or go for heap with malloc
    char buf[size];
    char *pbuf = buf;
    for (char **ptr = &action_argv[1]; *ptr != NULL; ptr++){
        if(NULL != memccpy(pbuf, *ptr, '\0', size)){
            pbuf += strlen(*ptr);
            *pbuf = ' ';
            pbuf += 1;
        }
    }
    *pbuf = '\0';
    DEBUG("Remaining non-common arguments: %s", buf);
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
    init_logging(stderr);
    // Keep it simple and allocate enough space for pointers to all
    // arguments in argv since we don't know how many of the options
    // are tools options.
    char **action_argv = malloc(argc * sizeof(void *));
    int action_argc;
    parse_common_arguments(argc, argv, &action_argc, action_argv);

    switch(args.action){
        case ACTION_UPDATE:
            int tool_argc;
            char **tool_argv = malloc(action_argc * sizeof(void *));
            parse_mdfu_update_arguments(action_argc, action_argv, &tool_argc, tool_argv);
            mdfu_update(tool_argc, tool_argv);
            break;
        case ACTION_CLIENT_INFO:
            mdfu_client_info(action_argc, action_argv);
            break;
        case ACTION_TOOLS_HELP:
            tools_help();
            break;
    }
    exit(0);
}

