#include <errno.h>
#include "unity.h"
#include "cmock.h"
#include "transport/serial_transport.c"
#include "mock_mac_functions.h"
#include "mock_timeout.h"
#include "mock_checksum.h"
#include "logging.h"

static mac_t mock_mac;
static timeout_t mock_timer;

void setUp(void) {
    // Initialize the mock MAC layer
    mock_mac.open = mac_open;
    mock_mac.close = mac_close;
    mock_mac.read = mac_read;
    mock_mac.write = mac_write;
    mock_mac.init = mac_init;
    init_logging(stdout);
    set_debug_level(DEBUGLEVEL);
}

void tearDown(void) {
    // This is run after EACH test
}

void test_init(void) {
    int result = init(&mock_mac, 1000);
    TEST_ASSERT_EQUAL(0, result);
}

void test_open(void) {
    mac_open_ExpectAndReturn(0);
    int result = open();
    TEST_ASSERT_EQUAL(0, result);
}

void test_close(void) {
    mac_close_ExpectAndReturn(0);
    int result = close();
    TEST_ASSERT_EQUAL(0, result);
}

void test_discard_until(void) {
    uint8_t code = FRAME_START_CODE;
    mac_read_ExpectAnyArgsAndReturn(1);
    mac_read_ReturnThruPtr_data(&code);
    int result = discard_until(code, mock_timer);
    TEST_ASSERT_EQUAL(0, result);
}

void test_process_byte(void) {
    uint8_t tmp = ESCAPE_SEQ_CODE;
    uint8_t data[1];
    uint8_t *pdata = data;
    bool escape_code = false;

    int result = process_byte(tmp, &pdata, &escape_code);
    TEST_ASSERT_EQUAL(0, result);
    TEST_ASSERT_TRUE(escape_code);
}

void test_read_and_decode_until(void) {
    uint8_t data[16];
    uint8_t code = FRAME_END_CODE;
    mac_read_ExpectAnyArgsAndReturn(1);
    mac_read_ReturnThruPtr_data(&code);
    ssize_t result = read_and_decode_until(16, data, mock_timer);
    TEST_ASSERT_EQUAL(0, result);
}

void test_encode_and_send(void) {
    uint8_t data[1] = {FRAME_START_CODE};
    uint8_t encoded_data[2] = {ESCAPE_SEQ_CODE, FRAME_START_ESC_SEQ};
    mac_write_ExpectAndReturn(2, encoded_data, 0);
    int result = encode_and_send(1, data);
    TEST_ASSERT_EQUAL(0, result);
}

int mac_read_callback(int size, uint8_t* data, int cmock_num_calls){
    uint8_t frame[] = {FRAME_START_CODE, 0x00, 0x01, 0x02, 0x03, FRAME_END_CODE};
    TEST_ASSERT_EQUAL(1, size);
    TEST_ASSERT(cmock_num_calls <= sizeof(frame));
    *data = frame[cmock_num_calls - 1]; 
    return 1;
}

bool timeout_expired_callback(timeout_t* timer, int cmock_num_calls){
    return false;
}

void test_read(void) {
    uint8_t data[MDFU_CMD_PACKET_MAX_SIZE];
    int size;
    set_timeout_IgnoreAndReturn(0);
    mac_read_StubWithCallback(mac_read_callback);
    timeout_expired_StubWithCallback(timeout_expired_callback);

    calculate_crc16_IgnoreAndReturn(0x0302);
    int result = read(&size, data, 1.0);
    TEST_ASSERT_EQUAL(0, result);
}

void test_write(void) {
    uint8_t data[1] = {0x01};
    uint16_t checksum = 0x1234;
    uint8_t frame_start_code = FRAME_START_CODE;
    uint8_t frame_end_code = FRAME_END_CODE;
    uint8_t encoded_checksum[2] = {0x34, 0x12};

    calculate_crc16_ExpectAndReturn(1, data, checksum);
    mac_write_ExpectAndReturn(1, &frame_start_code, 0);
    mac_write_ExpectAndReturn(1, data, 0);
    mac_write_ExpectAndReturn(1, &encoded_checksum[0], 0);
    mac_write_ExpectAndReturn(1, &encoded_checksum[1], 0);
    mac_write_ExpectAndReturn(1, &frame_end_code, 0);

    int result = write(1, data);
    TEST_ASSERT_EQUAL(0, result);
}