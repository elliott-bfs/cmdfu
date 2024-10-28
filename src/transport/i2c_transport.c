/**
 * @file i2c_transport.c
 * @brief I2C transport layer.
 */
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <assert.h>
#include "mdfu/error.h"
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
static mac_t *transport_mac;

static timeout_t itd_timer;
static float itd_delay = 0.01;

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
    transport_mac = mac;
    set_timeout(&itd_timer, 0);
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
    return transport_mac->open();
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
    return transport_mac->close();
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
    int status = 0;
    
    if(create_cmd_frame(size, data, &frame_size, buffer) < 0){
        return -1;
    }
    DEBUG("I2C transport sending frame: ");
    log_frame(frame_size, buffer);
    while(!timeout_expired(&itd_timer));
    status = transport_mac->write(frame_size, buffer);
    // Ignore errors on write as defined in MDFU spec
    // Error will be detected once polling for a response 
    if(status < 0){
        DEBUG("I2C transport error on sending command");
        status = 0;
    }
    if(0 > set_timeout(&itd_timer, itd_delay)){
        status = -1;
    }
    return status;
}

/**
 * @brief Poll for a client response length
 *
 * @param timer Pointer to the transport read operation timeout
 * @return ssize_t -1 for an error, otherwise the response length
 */
static ssize_t poll_for_client_response_length(timeout_t *timer){
    int data_size = -1;

    // Poll for a client response
    while(true){
        while(!timeout_expired(&itd_timer));

        DEBUG("Polling client for response length");
        if(transport_mac->read(RSP_LENGTH_FRAME_SIZE, buffer) < 0){
            set_timeout(&itd_timer, itd_delay);
            if(timeout_expired(timer)){
                DEBUG("Timeout during polling for response length");
                return -TIMEOUT_ERROR;
            }
            continue; // TODO for some errors we may want to terminate this loop. Need to define some non-recoverable MAC error codes
        }
        if(0 > set_timeout(&itd_timer, itd_delay)){
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
                return -CHECKSUM_ERROR;
            }
            break;
        }

        if(timeout_expired(timer)){
            DEBUG("Timeout during polling for response length");
            return -TIMEOUT_ERROR;
        }
    }
    return data_size;
}

/**
 * @brief Poll for a client response
 *
 * @param timer Pointer to transport read timeout
 * @param response_length Expected length of the response
 * @param data Pointer to buffer where response data should be stored
 * @return ssize_t 0 for success, -1 for error
 */
static int poll_for_client_response(timeout_t *timer, int response_length, uint8_t *data){
    while(true){
        while(!timeout_expired(&itd_timer));

        if(transport_mac->read(FRAME_TYPE_SIZE + response_length, buffer) < 0){
            set_timeout(&itd_timer, itd_delay);
            if(timeout_expired(timer)){
                DEBUG("Timeout during polling for response");
                return -TIMEOUT_ERROR;
            }
            continue; // TODO for some errors we may want to terminate this loop. Need to define some non-recoverable MAC error codes
        }
        set_timeout(&itd_timer, itd_delay);
        if(rsp_frame_type_response == buffer[0]){

            uint16_t checksum = *((uint16_t *) &buffer[FRAME_TYPE_SIZE + response_length - FRAME_CHECKSUM_SIZE]);
            uint16_t calc_checksum = calculate_crc16(response_length - FRAME_CHECKSUM_SIZE, &buffer[FRAME_TYPE_SIZE]);
            if(checksum != calc_checksum){
                ERROR("I2C transport frame checksum mismatch");
                return -CHECKSUM_ERROR;
            }
            memcpy(data, &buffer[FRAME_TYPE_SIZE], response_length - FRAME_CHECKSUM_SIZE);
            break;
        }
        if(timeout_expired(timer)){
            DEBUG("Timeout during polling for response");
            return -TIMEOUT_ERROR;
        }
    }
    return 0;
}

/**
 * @brief Transport read
 *
 * Retrieves a MDFU response
 *
 * @param size Pointer for storing the response size
 * @param data Pointer for storing the response data
 * @param timeout Timeout for the transport read operation in seconds.
 * @return int Read operation status, 0 for success, -1 for error
 */
static int read(int *size, uint8_t *data, float timeout){
    timeout_t timer;
    int response_length;
    int status;

    // Poll for a client response
    DEBUG("Starting client response length polling");
    set_timeout(&timer, timeout);
    response_length = poll_for_client_response_length(&timer);
    if(response_length < 0){
        return response_length;
    }
    *size = response_length - FRAME_CHECKSUM_SIZE;
    DEBUG("Starting client response polling");
    status = poll_for_client_response(&timer, response_length, data);
    if(status < 0){
        return status;
    }
    return 0;
}

/**
 * @brief Transport control
 *
 * @param request Transport control request
 * Request                         | Argument(s)
 * MAC_IOC_INTER_TRANSACTION_DELAY | float
 * @param ... Arguments for the control request
 * @return int
 */
static int ioctl(int request, ...){
    va_list args;
    va_start(args, request);
    if(TRANSPORT_IOC_INTER_TRANSACTION_DELAY == request){
        itd_delay = (float) va_arg(args, double);
        return 0;
    }else{
        return -1;
    }
    va_end(args);
}

transport_t i2c_transport ={
    .close = close,
    .open = open,
    .read = read,
    .write = write,
    .init = init,
    .ioctl = ioctl
};

int get_i2c_transport(transport_t **transport){
    *transport = &i2c_transport;
    return 0;
}