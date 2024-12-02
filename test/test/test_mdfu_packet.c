#include <stdbool.h>
#include <stdio.h>
#include <unity.h>
#include "mdfu/mdfu.h"
#include "mdfu/logging.h"

extern int mdfu_get_packet_buffer(mdfu_packet_t *cmd_packet, mdfu_packet_t *status_packet);

void setUp(void){
    init_logging(stderr);
    set_debug_level(DEBUGLEVEL);
}

void tearDown(void){

}

void test_mdfu_packet(void){
    /* Packet with - Sync = true, Command = Get Client Info, Data = 0x11223344 */
    uint8_t cmd_packet[] = {0x81, 0x01, 0x11, 0x22, 0x33, 0x44};
    uint8_t buffer[] = {0x00, 0x00, 0x11, 0x22, 0x33, 0x44};
    mdfu_packet_t decoded_packet = {
        .buf = (uint8_t *) cmd_packet
    };
    mdfu_packet_t packet = {
        .sequence_number = 1,
        .sync = true,
        .resend = false,
        .command = GET_CLIENT_INFO,
        .data_length = 4,
        .data = &buffer[2],
        .buf = (uint8_t *) &buffer
    };
    int size = sizeof(cmd_packet);

    // Log a packet
    mdfu_log_packet(&packet, MDFU_CMD);

    mdfu_encode_cmd_packet(&packet);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(cmd_packet, packet.buf, 6);
    mdfu_decode_packet(&decoded_packet, MDFU_CMD, size);

    TEST_ASSERT_EQUAL_INT8(packet.sequence_number, decoded_packet.sequence_number);
    TEST_ASSERT_EQUAL_INT8(packet.command, decoded_packet.command);
    TEST_ASSERT_EQUAL(packet.sync, decoded_packet.sync);
    TEST_ASSERT_EQUAL_INT16(packet.data_length, size - 2);

}
