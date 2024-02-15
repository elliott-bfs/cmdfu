#pragma once
#include <getopt.h>
#include "tools.h"
#include "mac.h"

typedef struct {
    int (* get_transport)(int, char **, transport_t *);
    void (* list_supported_tools)(void);
}tool_t;

#define TOOL_PARAMETERS_HELP "\
--host <host>: e.g. 127.0.0.1\n\
--port <port> e.g. 5559\n"

static int parse_args(int argc, char **argv, struct socket_config *conf){
    int opt;
    static struct option long_options[] =
    {
        {"host", required_argument, 0, 'h'},
        {"port", required_argument, 0, 'p'},
        {0, 0, 0, 0}
    };
    while (1)
    {
        /* getopt_long stores the option index here. */
        int option_index = 0;

        opt = getopt_long(argc, argv, "h:p:",
                        long_options, &option_index);

        /* Detect the end of the options. */
        if (opt == -1)
            break;

        switch (opt) {
            case 0:
                puts("Should not happen");
                break;

            case 'h':
                printf("option -h with value `%s'\n", optarg);
                conf->host = malloc(strlen(optarg));
                strcpy(conf->host, optarg);
                break;

            case 'p':
                printf("option -p with value `%s'\n", optarg);
                conf->port = atoi(optarg);
                break;

            case '?':
                /* getopt_long already printed an error message. */
                break;

            default:
                return -1;
        }
    }
    return 0;
}

static int get_transport(int argc, char **argv, transport_t *transport){
    mac_t mac;
    struct socket_config conf = {
        .host = "127.0.0.1",
        .port = 5559
    };
    if(parse_network_args(argc, argv, &conf) < 0)
    {
        return -1;
    }
    puts("Initializing stack");
    get_socket_mac(&mac);
    if(mac.init((void *) &conf) < 0) {
        puts("Socket MAC init failed");
        return -1;
    }
    get_serial_transport(transport);
    transport->init(&mac, 2);
    return 0;

}

static char *get_help(void){
    return TOOL_PARAMETERS_HELP;
}