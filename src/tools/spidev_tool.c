#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include "mdfu/tools/tools.h"
#include "mdfu/transport/transport.h"
#include "mdfu/logging.h"
#include "mdfu/mac/spidev_mac.h"

#define TOOL_PARAMETERS_HELP "\
Serial Tool Options:\n\
    --clk-speed <clock speed>: e.g. 1000000\n\
    --dev <device> e.g. /dev/spidev0.0\n\
    --mode <mode> One of [0, 1, 2, 3]\n"

static mac_t *spidev_mac = NULL;
static transport_t *spidev_transport = NULL;

static int open(void){
    DEBUG("Opening spidev tool");
    if(spidev_transport != NULL){
        return spidev_transport->open();
    }
    return -1;
}

static int close(void){
    DEBUG("Closing spidev tool");
    if(spidev_transport != NULL){
        return spidev_transport->close();
    }
    return -1;
}

static int init(void *config){
    struct spidev_config *spidev_conf = (struct spidev_config *) config;
    int status = 0;
    DEBUG("Initializing spidev tool");
    get_spidev_mac(&spidev_mac);
    if(0 == status){
        status = spidev_mac->init((void *) spidev_conf);
        if(status < 0){
            ERROR("spidev MAC init failed");
        }
        if(0 == status){
            status = get_transport(SPI_TRANSPORT, &spidev_transport);
            if(0 == status){
                status = spidev_transport->init(spidev_mac, 2);
            }
        }
    }
    if(status < 0){
        spidev_mac = NULL;
        spidev_transport = NULL;
    }
    return status;
}

static int read(int *size, uint8_t *data, float timeout){
    if(spidev_transport != NULL){
        return spidev_transport->read(size, data, timeout);
    } else{
        return -1;
    }
}

static int write(int size, uint8_t *data){
    if(spidev_transport != NULL){
        return spidev_transport->write(size, data);
    } else {
        return -1;
    }
}

static int parse_arguments(int tool_argc, char **tool_argv, void **config){
    int opt;
    static struct option long_options[] =
    {
        {"clk-speed", required_argument, NULL, 'b'},
        {"dev", required_argument, NULL, 'p'},
        {"mode", required_argument, NULL, 'm'},
        // Indicator for end of options list
        {0, 0, 0, 0}
    };
    bool error_exit = false;
    *config = calloc(sizeof(struct spidev_config), 1);
    struct spidev_config *spidev_conf = (struct spidev_config *) *config;
    
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
                spidev_conf->speed = atoi(optarg);
                break;

            case 'm':
                int mode = atoi(optarg);
                if(mode < 0 || mode > 3){
                    ERROR("Invalid SPI mode %d. Valid modes are 0, 1, 2 and 3.", mode);
                    error_exit = true;
                    break;
                }
                spidev_conf->mode = mode;
                break;

            case 'p':
                spidev_conf->path = malloc(strlen(optarg) + 1);
                strcpy(spidev_conf->path, optarg);
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
    if(0 == spidev_conf->speed){
        ERROR("The following arguments are required: --tool");
        return -1;
    }
    if(NULL == spidev_conf->path){
        ERROR("No spidev device was provided");
        return -1;
    }
    return 0;
}

static char *get_parameter_help(void){
    return TOOL_PARAMETERS_HELP;
}

tool_t spidev_tool = {
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