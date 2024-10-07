#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include "mdfu/tools/tools.h"
#include "mdfu/transport/transport.h"
#include "mdfu/logging.h"
#include "mdfu/mac/i2cdev_mac.h"

#define TOOL_PARAMETERS_HELP "\
Serial Tool Options:\n\
    --address <address>: e.g. 55\n\
    --dev <device> e.g. /dev/i2c-0"

static mac_t *i2cdev_mac = NULL;
static transport_t *i2cdev_transport = NULL;

static int open(void){
    DEBUG("Opening i2cdev tool");
    if(i2cdev_transport != NULL){
        return i2cdev_transport->open();
    }
    return -1;
}

static int close(void){
    DEBUG("Closing i2cdev tool");
    if(i2cdev_transport != NULL){
        return i2cdev_transport->close();
    }
    return -1;
}

static int init(void *config){
    struct spidev_config *i2cdev_conf = (struct spidev_config *) config;
    int status = 0;
    DEBUG("Initializing i2cdev tool");
    get_i2cdev_mac(&i2cdev_mac);
    if(0 == status){
        status = i2cdev_mac->init((void *) i2cdev_conf);
        if(status < 0){
            ERROR("i2cdev MAC init failed");
        }
        if(0 == status){
            status = get_transport(I2C_TRANSPORT, &i2cdev_transport);
            if(0 == status){
                status = i2cdev_transport->init(i2cdev_mac, 2);
            }
        }
    }
    if(status < 0){
        i2cdev_mac = NULL;
        i2cdev_transport = NULL;
    }
    return status;
}

static int read(int *size, uint8_t *data, float timeout){
    if(i2cdev_transport != NULL){
        return i2cdev_transport->read(size, data, timeout);
    } else{
        return -1;
    }
}

static int write(int size, uint8_t *data){
    if(i2cdev_transport != NULL){
        return i2cdev_transport->write(size, data);
    } else {
        return -1;
    }
}

static int parse_arguments(int tool_argc, char **tool_argv, void **config){
    int opt;
    static struct option long_options[] =
    {
        {"address", required_argument, NULL, 'a'},
        {"dev", required_argument, NULL, 'p'},
        // Indicator for end of options list
        {0, 0, 0, 0}
    };
    bool error_exit = false;
    *config = calloc(sizeof(struct i2cdev_config), 1);
    struct i2cdev_config *i2cdev_conf = (struct i2cdev_config *) *config;
    
    i2cdev_conf->address = -1;
    // Setting optind to zero triggers a re-initialization of the getopt parsing
    // library. This also sets the optind to the default value of 1 after the
    // initialization, thus we have a dummy value as first element in the array.
    optind = 0;
    opterr = 1;
    while (1)
    {
        int option_index = 0;

        opt = getopt_long(tool_argc, tool_argv, "a:p:",
                        long_options, &option_index);

        /* Detect the end of the options. */
        if (opt == -1){
            break;
        }

        switch (opt) {
            case 'a':
                i2cdev_conf->address = atoi(optarg);
                if(i2cdev_conf->address < 0 || i2cdev_conf->address > 0x7f){
                    ERROR("I2C address must be within 0 and 127");
                    error_exit = true;
                }
                break;

            case 'p':
                i2cdev_conf->path = malloc(strlen(optarg) + 1);
                strcpy(i2cdev_conf->path, optarg);
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
    if(i2cdev_conf->address < 0){
        ERROR("The following arguments are required: --address");
        return -1;
    }
    if(NULL == i2cdev_conf->path){
        ERROR("No i2cdev device was provided");
        return -1;
    }
    return 0;
}

static char *get_parameter_help(void){
    return TOOL_PARAMETERS_HELP;
}

tool_t i2cdev_tool = {
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