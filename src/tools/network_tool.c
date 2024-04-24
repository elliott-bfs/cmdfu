#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "mdfu/tools.h"
#include "mdfu/tools/network.h"
#include "mdfu/mac.h"
#include "mdfu/socket_mac.h"
#include "mdfu/logging.h"

#define TOOL_PARAMETERS_HELP "\
--host <host>: e.g. 127.0.0.1\n\
--port <port> e.g. 5559\n"

static mac_t *net_mac = NULL;
static transport_t *net_transport = NULL;

static int open(void){
    DEBUG("Opening network tool");
    if(net_transport != NULL){
        return net_transport->open();
    }
    return -1;
}

static int close(void){
    DEBUG("Closing network tool");
    if(net_transport != NULL){
        return net_transport->close();
    }
    return -1;
}

static int init(void *config){
    struct network_config *net_conf = (struct network_config *) config;
    int status = 0;
    DEBUG("Initializing network tool");
    get_socket_mac(&net_mac);
    if(0 == status){
        status = net_mac->init((void *) &net_conf->socket_config);
        if(status < 0){
            ERROR("Socket MAC init failed");
        }
        if(0 == status){
            status = get_transport(SOCKET_TRANSPORT, net_transport);
            if(0 == status){
                status = net_transport->init(net_mac, 2);
            }
        }
    }
    if(status < 0){
        net_mac = NULL;
        net_transport = NULL;
    }
    return status;
}

static int read(int *size, uint8_t *data){
    if(net_transport != NULL){
        return net_transport->read(size, data);
    } else{
        return -1;
    }
}

static int write(int size, uint8_t *data){
    if(net_transport != NULL){
        return net_transport->write(size, data);
    } else {
        return -1;
    }
}

static int parse_arguments(int tool_argc, char **tool_argv, void **config){
    int opt;
    static struct option long_options[] =
    {
        {"host", required_argument, 0, 'h'},
        {"port", required_argument, 0, 'p'},
        // Indicator for end of options list
        {0, 0, 0, 0}
    };
    *config = malloc(sizeof(struct network_config));
    struct network_config *net_conf = (struct network_config *) *config;
    
    // Setting optind to zero triggers a re-initialization of the getopt parsing
    // library. This also sets the optind to the default value of 1 after the
    // initialization, thus we have a dummy value as first element in the array.
    optind = 0;
    while (1)
    {
        int option_index = 0;

        opt = getopt_long(tool_argc, tool_argv, "h:p:",
                        long_options, &option_index);

        /* Detect the end of the options. */
        if (opt == -1){
            break;
        }

        switch (opt) {
            case 'h':
                net_conf->socket_config.host = malloc(strlen(optarg) + 1);
                strcpy(net_conf->socket_config.host, optarg);
                break;

            case 'p':
                net_conf->socket_config.port = atoi(optarg);
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

static char *get_help(void){
    return TOOL_PARAMETERS_HELP;
}

tool_t network_tool = {
    .open = open,
    .close = close,
    .init = init,
    .write = write,
    .read = read,
    .parse_arguments = parse_arguments
};
