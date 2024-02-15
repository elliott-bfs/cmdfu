#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <getopt.h>
#include "version.h"
#include "mdfu/socket_mac.h"
#include "mdfu/serial_transport.h"
#include "mdfu/transport.h"

int verbose_flag;

uint8_t get_transfer_parameters_frame[] = {0x50, 0x48, 0x43, 0x4d, 0x02, 0x00, 0x80, 0x01, 0x7d, 0xfe};
#define GET_TRANSFER_PARAMETERS_RETURN_FRAME_SIZE (4 + 2 + 5 + 2)
uint8_t get_transfer_parameters_mdfu_packet[] = {0x80, 0x01};

int parse_network_args(int argc, char **argv, struct socket_config *conf){
    int opt;
    static struct option long_options[] =
    {
        {"host", required_argument, 0, 'h'},
        {"port", required_argument, 0, 'p'},
        {0, 0, 0, 0}
    };
    puts("Parsing network options");
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

void config_mac(void){
    mac_t mac;
    struct socket_config conf = {
        .host = "10.146.84.54",
        .port = 5559
    };
    get_socket_mac(&mac);
    mac.init((void *) &conf);
    mac.open();
    // mac.close();
    mac.write(sizeof(get_transfer_parameters_frame), get_transfer_parameters_frame);
    uint8_t buffer[1024];
    mac.read(GET_TRANSFER_PARAMETERS_RETURN_FRAME_SIZE, buffer);
    
    for(int i=0; i < GET_TRANSFER_PARAMETERS_RETURN_FRAME_SIZE; i++)
    {
        printf("%02x", buffer[i]);
    }
    printf("\n");
    fflush(NULL);
    mac.close();
}

static void connect(char *host, int port){
    mac_t mac;
    struct socket_config conf = {
        .host = "127.0.0.1",//host,//"10.146.89.27",
        .port = 5559//port //5559
    };
    puts("Initializing stack");
    get_socket_mac(&mac);
    if(mac.init((void *) &conf) < 0) {
        puts("Socket MAC init failed");
        return;
    }
    transport_t transport;
    get_serial_transport(&transport);
    transport.init(&mac, 2);

    if(transport.open() < 0){
        return;
    }
    transport.write(sizeof(get_transfer_parameters_mdfu_packet), get_transfer_parameters_mdfu_packet);
    int size = 0;
    uint8_t buffer[1024];
    int status = transport.read(&size, buffer);
    if(status < 0){
        printf("Transport error\n");
        transport.close();
        return;
    }
    transport.close();
}

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

static int get_network_transport(int argc, char **argv, transport_t *transport){
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

void test_factories(void){
    struct socket_config conf = {
        .host = "172.30.224.1",
        .port = 5559
    };
    mac_t mac;
    get_socket_mac(&mac);
    mac.init((void *) &conf);

    transport_t transport;
    get_transport(SERIAL_TRANSPORT, &transport);
    transport.init(&mac, 2);
}

int main(int argc, char **argv)
{
    int opt;
    static struct option long_options[] =
    {
        /* These options set a flag. */
        {"verbose", required_argument, NULL, 'v'},
        {"brief", no_argument, &verbose_flag, 0},
        {"version", no_argument, 0, 'V'},
        {"help", no_argument, 0, 'h'},
        /* These options don’t set a flag.
            We distinguish them by their indices. */
        {"add", no_argument, 0, 'a'},
        {"host", required_argument, 0, 0},
        {"port", required_argument, 0, 'p'},
        {0, 0, 0, 0}};
    char *host;
    int port;
    printf("Hello, from mdfu!\n");
    //printf("Version: %d.%d.%d\n", MDFU_VERSION_MAJOR, MDFU_VERSION_MINOR, MDFU_VERSION_PATCH);

    while (1)
    {
        /* getopt_long stores the option index here. */
        int option_index = 0;

        opt = getopt_long(argc, argv, "abv:p:hV:",
                        long_options, &option_index);

        /* Detect the end of the options. */
        if (opt == -1)
            break;

        switch (opt)
        {
        case 0:
            /* If this option set a flag, do nothing else now. */
            if (long_options[option_index].flag != 0)
                break;
            printf("option %s", long_options[option_index].name);
            if (optarg)
                printf(" with arg %s", optarg);
            printf("\n");
            break;
        case 'v':
            printf("Verbosity `%s'\n", optarg);
            break;
        case 'V':
            printf("Version: %d.%d.%d\n", MDFU_VERSION_MAJOR, MDFU_VERSION_MINOR, MDFU_VERSION_PATCH);
            break;
        case 'h':
            puts("TODO write help text");
            break;

        case 'p':
            printf("option -p with value `%s'\n", optarg);
            port = atoi(optarg);
            break;

        case '?':
            /* getopt_long already printed an error message. */
            break;

        default:
            abort();
        }
    }
    const char* actions[] = {"update", "read"};
    const char* tools[] = {"network", "serial"};
    #define ACTION_CNT 2
    #define ACTION_UPDATE 0
    #define ACTION_READ 1
    #define TOOLS_CNT 2
    int action = -1;
    int tool = -1;
    /* Print any remaining command line arguments (not options). */
    if (optind < argc)
    {
        for(int i=0; i < ACTION_CNT; i++){
            if(strcmp(argv[optind], actions[i]) == 0){
                action = i;
                break;
            }
        }
        if(action < 0){
            printf("Unknown action %s\n", argv[optind]);
        }
        optind++;
        if(optind < argc){

            for(int i=0; i < TOOLS_CNT; i++){
                if(strcmp(argv[optind], tools[i]) == 0){
                    tool = i;
                    break;
                }
            }
            if(tool < 0){
                printf("Unknown tool %s\n", argv[optind]);
            }
            optind++;
        }else{
            puts("Missing tool");
        }
    }else{
        puts("TODO wrtie help text");
    }
    switch(action){
        case ACTION_UPDATE:
            transport_t transport;
            get_network_transport(argc, argv, &transport);
            mdfu_update(&transport);
            break;
    }
    exit(0);
}

