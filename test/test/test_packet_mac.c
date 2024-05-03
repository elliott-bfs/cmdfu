#include <unity.h>
#include "mdfu/logging.h"
#include "mdfu/socket_mac.h"

void setUp(void){
    init_logging(stderr);
    set_debug_level(DEBUGLEVEL);
}

void tearDown(void){

}

void test_socket_mac(void){

    uint8_t message[] = "Hello World";//{0x80, 0x01};
    uint8_t buffer[128];
    mac_t *mac;

    struct socket_config conf = {
        .host = "10.146.89.27",
        .port = 5558
    };
    get_socket_mac(&mac);
    mac->init((void *) &conf);

    mac->open();
    //mac.write(sizeof(get_transfer_parameters_mdfu_packet), get_transfer_parameters_mdfu_packet);
    //mac.read(sizeof(message), buffer);

    mac->close();

    //TEST_ASSERT_EQUAL_STRING(&message, buffer);
}