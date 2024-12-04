#include "unity.h"
#include "transport/serial_transport.c"
#include "utils/checksum.c"
#include "mock_mac.h"
#include "timeout.h"
#include "logging.h"
//#include "mock_timeout.h" // If you have a separate timer module, mock it as well
int mac_read(int size, uint8_t *data);
void setUp(void) {
    // This is run before EACH test
}

void tearDown(void) {
    // This is run after EACH test
}

int mac_read(int size, uint8_t *data){
    return 0;
}

void test_read_and_decode_until_NoDataReceived_ShouldTimeout(void) {
    uint8_t buffer[10];
    timeout_t timer;
    mac_t mac;
    mac.read = mac_read;
    init_logging(stdout);
    set_timeout(&timer, 1);
    //mac_read_ExpectAndReturn(1, &buffer, 0); // Expect no data read
    //timeout_expired_ExpectAndReturn(timer, true); // Expect timeout
    init(&mac, 1);
    ssize_t result = read_and_decode_until(10, buffer, timer);

    TEST_ASSERT_EQUAL_INT(-1, result);
    TEST_ASSERT_EQUAL_INT(ETIMEDOUT, errno);

    
}
