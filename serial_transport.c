#include <stdint.h>
#include <stdbool.h>
#include <stdio.h> // printf
#include <errno.h>
#include "serial_transport.h"

#define FRAME_CHECK_SEQUENCE_SIZE 2
#define FRAME_PAYLOAD_LENGTH_SIZE 2
#define FRAME_START_CODE 0x56
#define FRAME_END_CODE 0x9E
#define ESCAPE_SEQ_CODE 0xCC
#define FRAME_START_ESC_SEQ (~FRAME_START_CODE & 0xff)
#define FRAME_END_ESC_SEQ  (~FRAME_END_CODE & 0xff)
#define ESCAPE_SEQ_ESC_SEQ (~ESCAPE_SEQ_CODE & 0xff)


static mac_t transport_mac;
static int transport_timeout;
static uint8_t buffer[1024];

/**
 * @brief Calculate inverted 16-bit two's complement frame checksum.
 * 
 * Pads data implicitly with zero byte to calulate checksum.
 * This function will not work on big-endian machines.
 * 
 * @param [in] data - Pointer to data for checksum calculation.
 * @param [in] size - Number of bytes in data.
 * @return uint16_t - Frame check sequence.
 */
static uint16_t calculate_frame_checksum(int size, uint8_t *data)
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

int discard_until(uint8_t code)
{
    bool pattern_detected = false;
    int status;
    uint8_t data;
    uint8_t timeout = 5;
    // TODO proper timeout implementation
    //timer = Timer(self.timeout)
    while(!pattern_detected)// && not timer.expired())
    {
        status = transport_mac.read(1, &data);
        if(status < 0){
            // TODO: error logging handling here
            perror("Transport error with: ");
            break;
        }else if(status == 1) {
            if(data == code){
                pattern_detected = true;
            } else {
                break;
            }
        }
        //if timer.expired():
        //    raise TimeoutError("Timeout while waiting for frame start pattern")
    }
    if(pattern_detected){
        status = 0;
    }
    return status;
}

int read_until(uint8_t code, int *size, uint8_t *data){
    bool pattern_detected = false;
    int status = 0;
    uint8_t tmp;
    uint8_t *pdata = data;
    uint8_t timeout = 5;
    // TODO proper timeout implementation
    //timer = Timer(self.timeout)
    while(!pattern_detected)// && not timer.expired())
    {
        status = transport_mac.read(1, &tmp);

        if(status < 0){
            // TODO: error logging handling here
            perror("Transport error with: ");
            break;
        }else if(status == 1) {
            if(tmp == code){
                pattern_detected = true;
            } else {
                *pdata = tmp;
                pdata++;
            }
        }
        //if timer.expired():
        //    raise TimeoutError("Timeout while waiting for frame start pattern")
    }
    *size = pdata - data;
    if(pattern_detected){
        status = 0;
    }
    return status;
}

void service_transport(void)
{
    enum transport_state {
        FRAME_START_SYNC = 0,
        RECEIVE_FRAME = 1,
    };
    static enum transport_state state = FRAME_START_SYNC;
    switch(state)
    {
        case FRAME_START_SYNC:
            state = RECEIVE_FRAME;
            break;
        case RECEIVE_FRAME:
            state = FRAME_START_SYNC;
            break;
    }

}

static int init(mac_t *mac, int timeout){
    transport_mac.open = mac->open;
    transport_mac.close = mac->close;
    transport_mac.read = mac->read;
    transport_mac.write = mac->write;
    transport_mac.init = mac->init;
    transport_timeout = timeout;
    return 0;
}

static int open(void){
    return transport_mac.open();
}

static int close(void){
    return transport_mac.close();
}

static void encode_frame_payload(int data_size, uint8_t *data, uint8_t *encoded_data, int *encoded_data_size){
    uint8_t code;
    int size = 0;

    for(int i = 0; i < data_size; i++)
    {
        code = data[i];
        if(code == FRAME_START_CODE){
            encoded_data[size] = ESCAPE_SEQ_CODE;
            size += 1;
            encoded_data[size] = FRAME_START_ESC_SEQ;
            size += 1;
        } else if(code == FRAME_END_CODE){
            encoded_data[size] = ESCAPE_SEQ_CODE;
            size += 1;
            encoded_data[size] = FRAME_END_ESC_SEQ;
            size += 1;
        } else if(code == ESCAPE_SEQ_CODE){
            encoded_data[size] = ESCAPE_SEQ_CODE;
            size += 1;
            encoded_data[size] = ESCAPE_SEQ_ESC_SEQ;
            size += 1;
        } else {
            encoded_data[size] = code;
            size += 1;
        }
    }
    *encoded_data_size = size;
}

static int decode_frame_payload(int data_size, uint8_t *data, int *decoded_data_size, uint8_t *decoded_data){
    bool escape_code = false;
    uint8_t code;
    int size = 0;
    int status = 0;
    
    for(int i = 0; i < data_size; i++)
    {
        code = data[i];
        if(escape_code){
            if(code == FRAME_START_ESC_SEQ){
                decoded_data[size] = FRAME_START_CODE;
            } else if(code == FRAME_END_ESC_SEQ){
                decoded_data[size] = FRAME_END_CODE;
            } else if(code == ESCAPE_SEQ_ESC_SEQ){
                decoded_data[size] = ESCAPE_SEQ_CODE;
            } else {
                // TODO: Log error
                status = -1;
                errno = EINVAL;
                break;
            }
            size += 1;
        } else {
            if(code == ESCAPE_SEQ_CODE){
                escape_code = true;
            } else {
                decoded_data[size] = code;
                size += 1;
            }
        }
    }
    *decoded_data_size = size;
    return status;
}

static void log_frame(int size, uint8_t *data){
    int i = 0;
    printf("size=%d payload=0x", size);
    
    for(; i < size - 2; i++){
        printf("%02x", data[i]);
    }
    printf(" fcs=0x%04x\n", *((uint16_t *) &data[size - 2]));
}

static void print_hex_string(int size, uint8_t *buffer){
    for(int i = 0; i < size; i++){
        printf("%02x", buffer[i]);
    }
}

static int read(int *size, uint8_t *data){
    int status;
    uint16_t checksum;
    int decoded_size;

    status = discard_until(FRAME_START_CODE);
    if(status < 0){
        // TODO log error
        return status;
    }
    status = read_until(FRAME_END_CODE, size, buffer);
    if(status < 0){
        perror("Frame reception failed with: ");
        return status;
    }
    
    // decoding into the same buffer
    status = decode_frame_payload(*size, buffer, &decoded_size, data);
    if(status < 0){
        // log error
        return status;
    }
    *size = decoded_size;
    uint16_t frame_checksum = *((uint16_t *) &data[*size - 2]);
    printf("Got a frame: ");
    log_frame(*size, data);

    checksum = calculate_frame_checksum(*size - 2, data);
    if(checksum != frame_checksum){
        // TODO logging
        printf("Serial Transport: Frame check sequence verification failed, calculated 0x%04x but got 0x%04x\n", checksum, frame_checksum);
        return -1;
    }
    *size -= 2; // remove checksum size to get payload size
    return 0;
}

static int write(int size, uint8_t *data){
    int encoded_data_size = 0;
    int buf_index = 0;

    buffer[buf_index] = FRAME_START_CODE;
    buf_index += 1;

    uint16_t frame_check_sequence = calculate_frame_checksum(size, data);
    encode_frame_payload(size, data, &buffer[buf_index], &encoded_data_size);
    buf_index += encoded_data_size;
    encode_frame_payload(2, (uint8_t *) &frame_check_sequence, &buffer[buf_index], &encoded_data_size);
    buf_index += encoded_data_size;
    
    //*((uint16_t *) &buffer[buf_index]) = frame_check_sequence;
    //buf_index += 2;

    buffer[buf_index] = FRAME_END_CODE;
    buf_index += 1;
    printf("Sending frame: ");
    log_frame(buf_index - 2, &buffer[1]);
    return transport_mac.write(buf_index, buffer);
}

int get_serial_transport(transport_t *transport){
    transport->close = close;
    transport->open = open;
    transport->read = read;
    transport->write = write;
    transport->init = init;
}
