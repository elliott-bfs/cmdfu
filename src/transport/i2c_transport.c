/**
 * @file i2c_transport.c
 * @brief I2C transport layer.
 */
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include "mdfu/transport/i2c_transport.h"
#include "mdfu/timeout.h"
#include "mdfu/logging.h"
#include "mdfu_config.h"
#include "mdfu/mdfu.h"
#include "mdfu/checksum.h"


static const char rsp_frame_type_length = 'L';
static const char rsp_frame_type_response = 'R';

#define FRAME_TYPE_SIZE 1
#define FRAME_CHECKSUM_SIZE 2
#define RSP_LENGTH_FRAME_SIZE 5
#define RSP_LENGTH_FRAME_LENGTH_START 1
#define RSP_LENGTH_FRAME_CRC_START 3
#define RSP_LENGTH_FRAME_LENGTH_SIZE 2

#define FRAME_BUFFER_MAX_SIZE (FRAME_TYPE_SIZE + MDFU_SEQUENCE_FIELD_SIZE + MDFU_COMMAND_SIZE  + MDFU_MAX_COMMAND_DATA_LENGTH + FRAME_CHECKSUM_SIZE)
/** 
 * @brief MAC layer interface used for transport layer communication.
 * 
 * This variable holds the MAC layer interface.
 */
static mac_t transport_mac;

#ifdef MDFU_DYNAMIC_BUFFER_ALLOCATION
static uint8_t *buffer = NULL;
#else
/** 
 * @brief Buffer for storing data frames.
 * 
 * This buffer is used to store the data frames that are constructed or received
 * by the transport layer. The size of the buffer is calculated based on the
 * maximum expected size of the frames, including frame type field, sequence field,
 * command size, maximum command data length, and frame check sequence.
 */
static uint8_t buffer[FRAME_BUFFER_MAX_SIZE];
static uint8_t *pbuffer = buffer;
#endif

/**
 * @brief Initializes the transport layer.
 *
 * This function copies the MAC function pointers from the provided mac object
 * into the global transport_mac structure and sets the transport timeout.
 *
 * @param mac Pointer to a mac_t structure that contains the MAC layer function pointers.
 * @param timeout The default timeout value to be used for transport layer operations.
 * @return Always returns 0 to indicate success.
 */
static int init(mac_t *mac, int timeout){
    transport_mac.open = mac->open;
    transport_mac.close = mac->close;
    transport_mac.read = mac->read;
    transport_mac.write = mac->write;
    transport_mac.init = mac->init;
    return 0;
}

/**
 * @brief Opens the transport layer.
 *
 * This function calls the open method on the transport_mac object, which is responsible
 * for establishing the transport layer connection.
 *
 * @return int Returns 0 on success, or a non-zero error code on failure.
 */
static int open(void){
    return transport_mac.open();
}

/**
 * @brief Closes the transport layer.
 *
 * This function calls the close method on the transport_mac object, which is responsible
 * for closing the transport layer connection.
 *
 * @return int Returns 0 on success, or a non-zero error code on failure.
 */
static int close(void){
    return transport_mac.close();
}


static void log_frame(int size, uint8_t *data){
    int i = 0;
    if(DEBUGLEVEL > debug_level){
        return;
    }
    TRACE(DEBUGLEVEL, "size=%d payload=0x", size);
    
    for(; i < size; i++){
        TRACE(DEBUGLEVEL, "%02x", data[i]);
    }
    TRACE(DEBUGLEVEL, "\n");
}

static int create_cmd_frame(int size, uint8_t *data, int *frame_size, uint8_t *frame){
    int buf_index = 0;
    int i = 0;
    uint16_t frame_check_sequence;

    if(size > (sizeof(buffer) - FRAME_CHECKSUM_SIZE)){
        errno = EOVERFLOW;
        return -1;
    }

    for(i = 0;i < size; i++){
        frame[buf_index++] = data[i];
    }
    frame_check_sequence = calculate_crc16(size, data);
    frame[buf_index++] = (uint8_t) (frame_check_sequence & 0xff);
    frame[buf_index++] = (uint8_t) ((frame_check_sequence >> 8) & 0xff);
    *frame_size = buf_index;
    return 0;
}

static int write(int size, uint8_t *data){
    int frame_size = 0;
    
    if(create_cmd_frame(size, data, &frame_size, buffer) < 0){
        return -1;
    }
    DEBUG("I2C transport sending frame: ");
    log_frame(frame_size, buffer);
    return transport_mac.write(frame_size, buffer);
}

static ssize_t poll_for_client_response(float timeout){
    int frame_size;
    timeout_t timer;
    int data_size = -1;

    set_timeout(&timer, timeout);
    // Poll for a client response
    while(true){
        if(transport_mac.read(RSP_LENGTH_FRAME_SIZE, buffer) < 0){
            
            return -1;
        }
        DEBUG("I2C transport received frame: ");
        log_frame(RSP_LENGTH_FRAME_SIZE, buffer);
        if(rsp_frame_type_length == buffer[0]){

            data_size = *((uint16_t *) &buffer[RSP_LENGTH_FRAME_LENGTH_START]);
            uint16_t checksum = *((uint16_t *) &buffer[RSP_LENGTH_FRAME_CRC_START]);
            uint16_t calc_checksum = calculate_crc16(RSP_LENGTH_FRAME_LENGTH_SIZE, &buffer[RSP_LENGTH_FRAME_LENGTH_START]);
            if(checksum != calc_checksum){
                ERROR("I2C transport frame checksum mismatch");
                return -1;
            }
            break;
        }
        DEBUG("Received no response from client");
        if(timeout_expired(timer)){
            errno = ETIMEDOUT;
            return -1;
        }
    }
    return data_size;
}

static int read(int *size, uint8_t *data, float timeout){
    int frame_size;
    timer_t timer;
    int data_and_checksum_size, data_size;

    // Poll for a client response
    DEBUG("Start client response polling");
    data_and_checksum_size = poll_for_client_response(timeout);
    if(data_and_checksum_size < 0){
        DEBUG("Client response polling failed");
        return -1;
    }
    data_size = data_and_checksum_size - FRAME_CHECKSUM_SIZE;
    if(transport_mac.read(FRAME_TYPE_SIZE + data_and_checksum_size, buffer) < 0){
        return -1;
    }

    if(rsp_frame_type_response == buffer[0]){

        uint16_t checksum = *((uint16_t *) &buffer[FRAME_TYPE_SIZE + data_size]);
        uint16_t calc_checksum = calculate_crc16(data_size, &buffer[FRAME_TYPE_SIZE]);
        if(checksum != calc_checksum){
            ERROR("I2C transport frame checksum mismatch");
            return -1;
        }
    memcpy(data, &buffer[FRAME_TYPE_SIZE], data_size);
    *size = data_size;
    }
    return 0;
}

transport_t i2c_transport ={
    .close = close,
    .open = open,
    .read = read,
    .write = write,
    .init = init
};

int get_i2c_transport(transport_t **transport){
    *transport = &i2c_transport;
    return 0;
}