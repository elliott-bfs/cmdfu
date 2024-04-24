#include <stdbool.h>
#include <stdio.h>
#include <unity.h>
#include "mdfu/mdfu.h"
#include "mdfu/logging.h"

void setUp(void){
    // We need to have the logging initialized or the unit test framework will not compile in
    // the logging module
    init_logging(stderr);
    set_debug_level(DEBUGLEVEL);
}

void tearDown(void){

}

/**
 * @brief Test client info decoding with valid data.
 * 
 */
void test_client_info_decoding(void){
    
    uint8_t client_info_data[] = \
        {2, 3, 128, 128 >> 8, 2, // Parameter type=2 (buffer info),      size=3, buffer size=0x8000 (128), buffers=2
        1, 3, 1, 2, 3,           // Parameter type=1 (protocol version), size=3, 1=major, 2=minor, 3=patch
        3, 9,                    // Parameter type=3 (command timeouts), size=9
        0, 10, 0,                // Default timeout (0), timeout = 0x000A (10)
        3, 10, 0,                // Write chunk command (3) with timeout of 10
        4, 500 & 0xff, 500 >> 8}; // Get image state command (4) with timeout of 500
    client_info_t client_info;

    mdfu_decode_client_info((uint8_t *) &client_info_data, sizeof(client_info_data), &client_info);
    TEST_ASSERT_EQUAL_UINT8(2, client_info.buffer_count);
    TEST_ASSERT_EQUAL_UINT16(128, client_info.buffer_size);
    TEST_ASSERT_EQUAL_UINT8(1, client_info.version.major);
    TEST_ASSERT_EQUAL_UINT8(2, client_info.version.minor);
    TEST_ASSERT_EQUAL(false, client_info.version.internal_present);
    TEST_ASSERT_EQUAL_UINT16(10, client_info.cmd_timeouts[WRITE_CHUNK]);
    TEST_ASSERT_EQUAL_UINT16(500, client_info.cmd_timeouts[GET_IMAGE_STATE]);

}

/**
 * @brief Test client info decoding with invalid data.
 * 
 */
void test_client_info_decoding_error(void){
    uint8_t client_info_data[] = \
        {2, 3, 128, 128 >> 8, 2, // Parameter type=2 (buffer info),      size=3, buffer size=0x8000 (128), buffers=2
        1, 3, 1, 2, 3,           // Parameter type=1 (protocol version), size=3, 1=major, 2=minor, 3=patch
        3, 6,                    // Parameter type=3 (command timeouts), size=6
        0, 10, 0,                // Default timeout (0), timeout = 0x000A (10)
        3, 10, 0};               // Write chunk command (3) with timeout of 10
    client_info_t client_info;
    int status;

    // Invalid parameter type
    client_info_data[0] = 0xff;
    status = mdfu_decode_client_info((uint8_t *) &client_info_data, sizeof(client_info_data), &client_info);
    TEST_ASSERT_TRUE(status < 0);
    client_info_data[0] = 2;

    // Invalid size for parameter type 2
    client_info_data[1] = 4;
    status = mdfu_decode_client_info((uint8_t *) &client_info_data, sizeof(client_info_data), &client_info);
    TEST_ASSERT_TRUE(status < 0);

    // Invalid size for parameter type 2 (exceeds total number of client info bytes)
    client_info_data[1] = 32;
    status = mdfu_decode_client_info((uint8_t *) &client_info_data, sizeof(client_info_data), &client_info);
    TEST_ASSERT_TRUE(status < 0);
    client_info_data[1] = 3;

    // Invalid size for parameter type 2 (must be a multiple of 3)
    client_info_data[11] = 4;
    status = mdfu_decode_client_info((uint8_t *) &client_info_data, sizeof(client_info_data), &client_info);
    TEST_ASSERT_TRUE(status < 0);
    client_info_data[11] = 6;

    // Invalid command code in timeout parameter
    client_info_data[12] = 0xff;
    status = mdfu_decode_client_info((uint8_t *) &client_info_data, sizeof(client_info_data), &client_info);
    TEST_ASSERT_TRUE(status < 0);
    client_info_data[12] = 0x0;

    // Invalid index for default timeout parameter
    client_info_data[12] = 3; // Set write chunk command timeout as first parameter
    client_info_data[15] = 0; // Sed default timeout as second parameter
    status = mdfu_decode_client_info((uint8_t *) &client_info_data, sizeof(client_info_data), &client_info);
    TEST_ASSERT_TRUE(status < 0);   
}

void test_client_info_print(void){
    client_info_t client_info = {
        .buffer_count = 1,
        .buffer_size = 128,
        .version = {
            .major = 1,
            .minor = 2,
            .patch = 3,
            .internal = 12,
            .internal_present = true
        },
    };
    // Initialize timeouts with some values
    for(int i = 0; i < MAX_MDFU_CMD; i++){
        client_info.cmd_timeouts[i] = i * 10;
    }
    print_client_info(&client_info);
}