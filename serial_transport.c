#include <stdint.h>
#include <stdbool.h>
#include <stdio.h> // printf
#include "serial_transport.h"
#include "mac.h"

#define FRAME_CHECK_SEQUENCE_SIZE 2
#define FRAME_PAYLOAD_LENGTH_SIZE 2

static uint8_t frame_guard[] = {'P', 'H', 'C', 'M'};
static mac_t transport_mac;
static int transport_timeout;
static uint8_t buffer[1024];

/**
 * @brief Structure to hold a frame
 * 
 * No need for packing here since we won't send this
 * as is due to variable legth of payload
*/
typedef struct
{
    uint32_t guard;
    uint16_t payload_size;
    uint8_t * payload;
    uint16_t fcs;
} frame_t;


/**
 * @brief Calculate 16-bit two's complement frame checksum.
 * 
 * @param [in] frame - Frame of type frame_t.
 * @return uint16_t - Frame check sequence in host endianess.
 */
/* static uint16_t calculate_frame_checksum(uint16_t payload_size, uint8_t *payload)
{
    uint16_t checksum = 0U;

    checksum += payload_size;

    for (uint16_t index = 0; index < payload_size; index++)
    {
        uint8_t nextByte = payload[index];

        if ((index % 2) == 0)
        {
            checksum += (uint16_t) nextByte;
        }
        else
        {
            checksum += ((uint16_t) nextByte) << 8;
        }
    }
    return ~checksum;
}
 */
static uint16_t calculate_frame_checksum(uint16_t size, uint8_t *data)
{
    uint16_t checksum = 0U;

    for (uint16_t index = 0; index < size; index++)
    {
        uint8_t nextByte = data[index];

        if ((index % 2) == 0)
        {
            checksum += (uint16_t) nextByte;
        }
        else
        {
            checksum += ((uint16_t) nextByte) << 8;
        }
    }
    return ~checksum;
}

int read_until(void)
{
    bool pattern_detected = false;
    int status;
    uint8_t data;
    uint8_t timeout = 5;
    // TODO proper timeout implementation
    //timer = Timer(self.timeout)
    while(!pattern_detected && timeout)// && not timer.expired())
    {
        for(uint8_t i = 0; i < sizeof(frame_guard); i++)
        {
            status = transport_mac.read(1, &data);
            if(status < 0){
                break;
            }else if(status == 1) {
                if(data != frame_guard[i]){
                    break;
                } else if(i == sizeof(frame_guard) - 1){
                    pattern_detected = true;
                }
            }
        }
        timeout--;
        //if timer.expired():
        //    raise TimeoutError("Timeout while waiting for frame start pattern")
    }
    if(pattern_detected){
        return 0;
    } else {
        return -1;
    }
}

void service_transport(void)
{
    enum transport_state {
        GUARD_SYNC = 0,
        RECEIVE_DATA = 1
    };
    static enum transport_state state = GUARD_SYNC;
    switch(state)
    {
        case GUARD_SYNC:
            if(read_until())
            {
                state = RECEIVE_DATA;
            }
    }

}

int init(mac_t *mac, int timeout){
    transport_mac.open = mac->open;
    transport_mac.close = mac->close;
    transport_mac.read = mac->read;
    transport_mac.write = mac->write;
    transport_mac.init = mac->init;
    transport_timeout = timeout;
}

static int open(void){
    return transport_mac.open();
}

static int close(void){
    return transport_mac.close();
}

static int read(int *size, uint8_t *data){
    frame_t frame;
    // TODO check return code
    if(read_until() < 0){
        return -1;
    }
    transport_mac.read(2, buffer);
    // should do a ntoh here
    frame.payload_size = *((uint16_t *) buffer);
    uint16_t data_read = 0;

    // pass size by reference and update this if we get less
    uint16_t status = transport_mac.read(frame.payload_size + FRAME_CHECK_SEQUENCE_SIZE, &buffer[2]);
    if(status < 0){
        return -1;
    }
    frame.fcs = *((uint16_t *) &buffer[FRAME_PAYLOAD_LENGTH_SIZE + frame.payload_size]);
    printf("Got frame with: size=%d payload=0x", frame.payload_size);
    for(int i = 0; i < frame.payload_size; i++){
        printf("%02x", buffer[2+i]);
    }
    printf(" fcs=0x%04x\n", frame.fcs);
    uint16_t checksum = calculate_frame_checksum(FRAME_PAYLOAD_LENGTH_SIZE + frame.payload_size, buffer);
    if(checksum != frame.fcs){
        printf("Serial Transport: Frame check sequence verification failed");
        return -1;
    }
}

static int write(int size, uint8_t *data){
    int frame_size = sizeof(frame_guard) + FRAME_PAYLOAD_LENGTH_SIZE + size + FRAME_CHECK_SEQUENCE_SIZE;
    for(int i = 0; i < sizeof(frame_guard); i++)
    {
        buffer[i] = frame_guard[i];
    }
    
    *((uint16_t *) &buffer[4]) = (uint16_t) size;

    for(int i = 0; i < size; i++)
    {
        buffer[i + 6] = data[i];
    }
    uint16_t frame_check_sequence = calculate_frame_checksum(size + FRAME_PAYLOAD_LENGTH_SIZE, &buffer[4]);
    *((uint16_t *) &buffer[sizeof(frame_guard) + FRAME_CHECK_SEQUENCE_SIZE + size]) = frame_check_sequence;

    transport_mac.write(frame_size, buffer);
}

int get_serial_transport(transport_t *transport){
    transport->close = close;
    transport->open = open;
    transport->read = read;
    transport->write = write;
    transport->init = init;
}
