#include <stdbool.h>
#include <stdio.h>
#include <unity.h>
#include "mdfu/mdfu.h"
#include "mdfu/logging.h"

void setUp(void){
    init_logging(stderr);
    set_debug_level(DEBUGLEVEL);
}

void tearDown(void){

}

void test_mdfu_packet(void){
    mdfu_packet_buffer_t packet_buffer;
    /* Packet with - Sync = true, Command = Get Client Info, Data = 0x11223344 */
    uint8_t cmd_packet[] = {0x81, 0x01, 0x11, 0x22, 0x33, 0x44};

    mdfu_packet_t packet = {
        .sequence_number = 1,
        .sync = true,
        .resend = false,
        .command = GET_CLIENT_INFO,
        .data_length = 4,
        .data = (uint8_t *) &cmd_packet[2]
    };
    mdfu_packet_t decoded_packet;

    // Log a packet
    mdfu_log_packet(&packet, MDFU_CMD);

    mdfu_encode_cmd_packet(&packet,(uint8_t *) &packet_buffer.buffer, &packet_buffer.size);
    TEST_ASSERT_EQUAL(packet.data_length, 4);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(cmd_packet, packet_buffer.buffer, 6);
    mdfu_decode_packet(&decoded_packet, MDFU_CMD, (uint8_t *) &packet_buffer.buffer, packet_buffer.size);

    TEST_ASSERT_EQUAL_INT8(packet.sequence_number, decoded_packet.sequence_number);
    TEST_ASSERT_EQUAL_INT8(packet.command, decoded_packet.command);
    TEST_ASSERT_EQUAL(packet.sync, decoded_packet.sync);
    TEST_ASSERT_EQUAL_INT16(packet.data_length, packet_buffer.size - 2);

}
/*
void test_mdfu_packet_decoding_errors(void){
        mdfu_packet_buffer_t packet_buffer;
        uint8_t data[] = {0x11, 0x22, 0x33, 0x44};
        mdfu_packet_t packet = {
        .sequence_number = 44,
        .sync = true,
        .resend = false,
        .command = GET_CLIENT_INFO,
        .data_length = 4,
        .data = (uint8_t *) &data
    };
    mdfu_encode_cmd_packet_cp(&packet,(uint8_t *) &packet_buffer.buffer, &packet_buffer.size);
}
*/