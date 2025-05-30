#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include "mdfu/tools/tools.h"
#include "mdfu/tools/network.h"
#include "mdfu/logging.h"

#define TOOL_PARAMETERS_HELP "\
Networking Tool Options:\n\
    --host <host>: e.g. 127.0.0.1\n\
    --port <port>: e.g. 5559\n\
    --transport <transport>: Chose from serial, spi. Default is serial"

/** @brief MAC layer pointer */
static mac_t *net_mac = NULL;
/** @brief Transport layer pointer */
static transport_t *net_transport = NULL;

/**
 * @brief Networking tool initialization
 * 
 * @param config This is a struct network_config type for this tool but
 * passed in as an opaque object here so that the tool API is agnostic
 * to any specific tool implementations.
 * @return int -1 for error and 0 for success
 */
static int init(void *config){
    struct network_config *net_conf = (struct network_config *) config;
    int status = 0;

    DEBUG("Initializing network tool");

    if(SERIAL_TRANSPORT == net_conf->transport || SERIAL_TRANSPORT_BUFFERED == net_conf->transport ){
        get_socket_mac(&net_mac);
        status = net_mac->init((void *) &net_conf->socket_config);
        if(status < 0){
            ERROR("Socket MAC init failed");
        }
        if(0 == status){
            status = get_transport(net_conf->transport, &net_transport);
            if(0 == status){
                status = net_transport->init(net_mac, 2);
            }
        }
    }else if(SPI_TRANSPORT == net_conf->transport){
        DEBUG("Configuring SPI transport for network transport");
        get_socket_packet_mac(&net_mac);
        status = net_mac->init((void *) &net_conf->socket_config);
        if(status < 0){
            ERROR("Socket MAC init failed");
        }
        if(0 == status){
            status = get_transport(SPI_TRANSPORT, &net_transport);
            if(0 == status){
                status = net_transport->init(net_mac, 2);
            }
        }
    }else if(I2C_TRANSPORT == net_conf->transport){
        get_socket_packet_mac(&net_mac);
        status = net_mac->init((void * ) &net_conf->socket_config);
        if(status < 0){
            ERROR("Socket MAC init failed");
        }else{
            status = get_transport(I2C_TRANSPORT, &net_transport);
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

/**
 * @brief Start a session with the networking tool.
 * 
 * @return int -1 for error and 0 for success
 */
static int open(void){
    DEBUG("Opening network tool");
    if(net_transport != NULL){
        return net_transport->open();
    }
    return -1;
}

/**
 * @brief Close networking tool session.
 * 
 * @return int -1 for error and 0 for success
 */
static int close(void){
    DEBUG("Closing network tool");
    if(net_transport != NULL){
        return net_transport->close();
    }
    return -1;
}

/**
 * @brief Read MDFU response from networking tool.
 * 
 * @param size Pointer where the response size is written to
 * @param data Pointer where the response is written to
 * @param timeout Timeout in seconds for this read operation
 * @return int -1 for error and 0 for success
 */
static int read(int *size, uint8_t *data, float timeout){
    if(net_transport != NULL){
        return net_transport->read(size, data, timeout);
    } else{
        return -1;
    }
}

/**
 * @brief Write a MDFU command to networking tool.
 * 
 * @param size Size of the command
 * @param data Command
 * @return int -1 for error and 0 for success
 */
static int write(int size, uint8_t *data){
    if(net_transport != NULL){
        return net_transport->write(size, data);
    } else {
        return -1;
    }
}

/**
 * @brief Parse tool argument vector.
 *
 * Parses the argument vector for valid tool arguments and returns
 * a pointer to a configuration object that can be passed into
 * the tool initialization function.
 *
 * @param tool_argc Argument count
 * @param tool_argv Argument vector
 * @param config Pointer to tool configuration object
 * @return -1 for errors and 0 for success
 */
static int parse_arguments(int tool_argc, char **tool_argv, void **config){
    int opt;
    static struct option long_options[] =
    {
        {"host", required_argument, NULL, 'h'},
        {"port", required_argument, NULL, 'p'},
        {"transport", required_argument, NULL, 't'},
        // Indicator for end of options list
        {0, 0, 0, 0}
    };
    bool error_exit = false;
    *config = calloc(sizeof(struct network_config), 1);
    struct network_config *net_conf = (struct network_config *) *config;
    
    // Setting optind to zero triggers a re-initialization of the getopt parsing
    // library. This also sets the optind to the default value of 1 after the
    // initialization, thus we have a dummy value as first element in the array.
    optind = 0;
    // We handle error message printing directly
    opterr = 0;

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
                net_conf->socket_config.port = (uint16_t) atoi(optarg);
                break;
            case 't':
                if(0 == strcmp("serial", optarg)){
                    net_conf->transport = SERIAL_TRANSPORT;
                }else if(0 == strcmp("serial-buffered", optarg)){
                    net_conf->transport = SERIAL_TRANSPORT_BUFFERED;
                }else if(0 == strcmp("spi", optarg)){
                    net_conf->transport = SPI_TRANSPORT;
                }else if(0 == strcmp("i2c", optarg)){
                    net_conf->transport = I2C_TRANSPORT;
                }else{
                    ERROR("Unknown transport %s", optarg);
                    error_exit = true;
                }
                break;
            case '?':
                ERROR("Unrecognized option '%s'", tool_argv[optind - 1]);
                error_exit = true;
                break;
            default:
                error_exit = true;
                break;
        }
        if(error_exit){
            return -1;
        }
    }
    if(optind < tool_argc){
        ERROR("Invalid argument \"%s\"", tool_argv[optind]);
        return -1;
    }
    if(0 == net_conf->socket_config.port){
        WARN("No port was provided using 5559");
        net_conf->socket_config.port = 5559;
    }
    if(NULL == net_conf->socket_config.host){
        WARN("No host was provided using localhost");
        net_conf->socket_config.host = malloc(sizeof("localhost"));
        strcpy(net_conf->socket_config.host, "localhost");
    }
    return 0;
}
/**
 * @brief Get help on tools parameters.
 *
 * @return char* String with tool parameter help text
 */
static char *get_parameter_help(void){
    return TOOL_PARAMETERS_HELP;
}

tool_t network_tool = {
    .ops = {
        .open = open,
        .close = close,
        .init = NULL,
        .write = write,
        .read = read,
    },
    .init = init,
    .list_connected_tools = NULL,
    .parse_arguments = parse_arguments,
    .get_parameter_help = get_parameter_help
};