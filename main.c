#include <stdio.h>
#include <getopt.h>
#include "version.h"
#include "socket_mac.h"
#include "serial_transport.h"
#include "transport.h"

uint8_t get_transfer_parameters_frame[] = {0x50, 0x48, 0x43, 0x4d, 0x02, 0x00, 0x80, 0x01, 0x7d, 0xfe};
#define GET_TRANSFER_PARAMETERS_RETURN_FRAME_SIZE (4 + 2 + 5 + 2)

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

uint8_t get_transfer_parameters_mdfu_packet[] = {0x80, 0x01};

void config_transport(void){
    mac_t mac;
    struct socket_config conf = {
        .host = "10.146.89.27",
        .port = 5559
    };
    get_socket_mac(&mac);
    mac.init((void *) &conf);

    transport_t transport;
    get_serial_transport(&transport);
    transport.init(&mac, 2);
    transport.open();
    transport.write(sizeof(get_transfer_parameters_mdfu_packet), get_transfer_parameters_mdfu_packet);
    int size = 0;
    uint8_t buffer[1024];
    int status = transport.read(&size, buffer);
    if(status < 0){
        printf("Failed transport\n");
        transport.close();
        return;
    }
    for(int i=0; i < size; i++){
        printf("%02X", buffer[i]);
    }
    printf("\n");
    transport.close();
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

int main(int, char**){
    printf("Hello, from mdfu!\n");
    printf("Version: %d.%d.%d\n", MDFU_VERSION_MAJOR, MDFU_VERSION_MINOR, MDFU_VERSION_PATCH);
    //config_mac();
    config_transport();
}
