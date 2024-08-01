#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include "mdfu/tools.h"
#include "mdfu/transport.h"
#include "mdfu/logging.h"
#include "mdfu/serial_mac.h"

#define TOOL_PARAMETERS_HELP "\
Serial Tool Options:\n\
    --baudrate <baudrate>: e.g. 9600\n\
    --port <port> e.g. /dev/ttyACM0\n"

static mac_t *serial_mac = NULL;
static transport_t *serial_transport = NULL;

static int open(void){
    DEBUG("Opening serial tool");
    if(serial_transport != NULL){
        return serial_transport->open();
    }
    return -1;
}

static int close(void){
    DEBUG("Closing serial tool");
    if(serial_transport != NULL){
        return serial_transport->close();
    }
    return -1;
}

static int init(void *config){
    struct serial_config *serial_conf = (struct serial_config *) config;
    int status = 0;
    DEBUG("Initializing serial tool");
    get_serial_mac(&serial_mac);
    if(0 == status){
        status = serial_mac->init((void *) &serial_conf);
        if(status < 0){
            ERROR("Serial MAC init failed");
        }
        if(0 == status){
            status = get_transport(SERIAL_TRANSPORT, &serial_transport);
            if(0 == status){
                status = serial_transport->init(serial_mac, 2);
            }
        }
    }
    if(status < 0){
        serial_mac = NULL;
        serial_transport = NULL;
    }
    return status;
}

static int read(int *size, uint8_t *data, float timeout){
    if(serial_transport != NULL){
        return serial_transport->read(size, data, timeout);
    } else{
        return -1;
    }
}

static int write(int size, uint8_t *data){
    if(serial_transport != NULL){
        return serial_transport->write(size, data);
    } else {
        return -1;
    }
}

static int parse_arguments(int tool_argc, char **tool_argv, void **config){
    int opt;
    static struct option long_options[] =
    {
        {"baudrate", required_argument, NULL, 'b'},
        {"port", required_argument, NULL, 'p'},
        // Indicator for end of options list
        {0, 0, 0, 0}
    };
    bool error_exit = false;
    *config = calloc(sizeof(struct serial_config), 1);
    struct serial_config *serial_conf = (struct serial_config *) *config;
    
    // Setting optind to zero triggers a re-initialization of the getopt parsing
    // library. This also sets the optind to the default value of 1 after the
    // initialization, thus we have a dummy value as first element in the array.
    optind = 0;
    opterr = 1;
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
            case 'b':
                serial_conf->baudrate = atoi(optarg);
                break;

            case 'p':
                serial_conf->port = malloc(strlen(optarg) + 1);
                strcpy(serial_conf->port, optarg);
                break;

            case '?':
                ERROR("Error encountered during tool argument parsing");
                /* getopt_long already printed an error message. */
                error_exit = true;
                break;
            case ':':
                ERROR(" xx encountered during tool argument parsing");
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
    if(0 == serial_conf->baudrate){
        ERROR("No baudrate was provided using 115200");
        serial_conf->baudrate = 115200;
    }
    if(NULL == serial_conf->port){
        ERROR("No serial port was provided using ");
        return -1;
    }
    return 0;
}

static char *get_parameter_help(void){
    return TOOL_PARAMETERS_HELP;
}

tool_t serial_tool = {
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