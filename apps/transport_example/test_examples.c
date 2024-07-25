#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <getopt.h>
#include "mdfu/socket_mac.h"
#include "mdfu/serial_transport.h"
#include "mdfu/transport.h"
#include "mdfu/logging.h"
#include "mdfu/tools.h"
#include "mdfu/mdfu.h"

uint8_t get_client_info_frame[] = {0x56, 0x80, 0x01, 0x7f, 0xfe, 0x9e};
#define CLIENT_INFO_RETURN_FRAME_SIZE (4 + 2 + 5 + 2)
uint8_t client_info_mdfu_cmd_packet[] = {0x80, 0x01};

struct socket_config conf = {
    .host = "192.168.1.20",//"10.146.84.54",
    .port = 5559
};

void test_mac(void){
    mac_t *mac;

    get_socket_mac(&mac);
    mac->init((void *) &conf);
    mac->open();

    mac->write(sizeof(get_client_info_frame), get_client_info_frame);
    uint8_t buffer[1024];
    mac->read(CLIENT_INFO_RETURN_FRAME_SIZE, buffer);
    
    for(int i=0; i < CLIENT_INFO_RETURN_FRAME_SIZE; i++)
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
    transport->write(sizeof(client_info_mdfu_cmd_packet), client_info_mdfu_cmd_packet);
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

int main(int argc, char **argv){
    test_mac();
    test_transport();
    return 0;
}