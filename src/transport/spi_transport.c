/**
 * @file spi_transport.c
 * @brief SPI transport layer.
 */
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include "mdfu/transport/serial_transport.h"
#include "mdfu/timeout.h"
#include "mdfu/logging.h"
#include "mdfu_config.h"
#include "mdfu/mdfu.h"
#include "mdfu/checksum.h"

static const char frame_length_prefix[] = {'L', 'E', 'N'};
static const char frame_response_prefix[] = {'R', 'S', 'P'};

#define CLIENT_RSP_PREFIX_SIZE 4
#define CLIENT_RSP_LEN_LENGTH_SIZE 2
#define CLIENT_RSP_LEN_LENGTH_START 4
#define CLIENT_RSP_LEN_CHECKSUM_START 6
#define CLIENT_RSP_RSP_PAYLOAD_START 4

#define FRAME_TYPE_CMD 0x11
#define FRAME_TYPE_RSP_RETRIEVAL 0x55
#define FRAME_TYPE_SIZE 1
#define FRAME_CHECKSUM_SIZE 2

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

    if(size > (sizeof(buffer) - FRAME_CHECKSUM_SIZE - FRAME_TYPE_SIZE)){
        errno = EOVERFLOW;
        return -1;
    }

    frame[buf_index++] = FRAME_TYPE_CMD;

    for(i = 0;i < size; i++){
        frame[buf_index++] = data[i];
    }
    frame_check_sequence = calculate_crc16(size, data);
    frame[buf_index++] = (uint8_t) (frame_check_sequence & 0xff);
    frame[buf_index++] = (uint8_t) ((frame_check_sequence >> 8) & 0xff);
    *frame_size = buf_index;
    return 0;
}

static int create_rsp_frame(int size, int *frame_size, uint8_t *frame){
    int buf_index = 0;

    if(size > (sizeof(buffer) - CLIENT_RSP_PREFIX_SIZE)){
        errno = EOVERFLOW;
        ERROR("SPI transport buffer to small to fit command");
        return -1;
    }
    frame[buf_index++] = FRAME_TYPE_RSP_RETRIEVAL;
    // Zero out remaining frame bytes. These are don't care but its for
    // better debugging etc. The number of zero bytes is the payload plus
    // the prefix minus one, since the first byte contains the
    // frame type
    for(int i = 0; i < (size + CLIENT_RSP_PREFIX_SIZE - 1); i++){
        frame[buf_index++] = 0x00;
    }
    *frame_size = buf_index;
    return 0;
}

static int spi_transfer(int size){
    int read_size;
    DEBUG("SPI transport sending frame: ");
    log_frame(size, buffer);
    if(transport_mac.write(size, buffer) < 0){
        return -1;
    }
    read_size = transport_mac.read(size, buffer);
    if(read_size < 0){
        return -1;
    }
    DEBUG("SPI transport received frame: ");
    log_frame(read_size, buffer);
    if(read_size != size){
        ERROR("SPI MAC layer read size did not match write size");
        return -1;
    }
    return 0;
}

static int write(int size, uint8_t *data){
    int frame_size = 0;
    
    if(create_cmd_frame(size, data, &frame_size, buffer) < 0){
        return -1;
    }
    return spi_transfer(frame_size);
}

static ssize_t poll_for_client_response(float timeout){
    int frame_size;
    timeout_t timer;
    int data_size = -1;

    set_timeout(&timer, timeout);
    // Poll for a client response
    while(true){
        if(create_rsp_frame(CLIENT_RSP_LEN_LENGTH_SIZE + FRAME_CHECKSUM_SIZE, &frame_size, buffer) < 0){
            DEBUG("failed to create response frame");
            return -1;
        }
        if(spi_transfer(frame_size) < 0){
            DEBUG("SPI transfer failed");
            return -1;
        }

        if(frame_length_prefix[0] == buffer[1] && 
            frame_length_prefix[1] == buffer[2] &&
            frame_length_prefix[2] == buffer[3]){

            data_size = *((uint16_t *) &buffer[CLIENT_RSP_LEN_LENGTH_START]);
            uint16_t checksum = *((uint16_t *) &buffer[CLIENT_RSP_LEN_CHECKSUM_START]);
            uint16_t calc_checksum = calculate_crc16(CLIENT_RSP_LEN_LENGTH_SIZE, &buffer[CLIENT_RSP_LEN_LENGTH_START]);
            if(checksum != calc_checksum){
                ERROR("SPI transport frame checksum mismatch");
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
    int data_size;

    // Poll for a client response
    DEBUG("Start client response polling");
    data_size = poll_for_client_response(timeout);
    if(data_size < 0){
        DEBUG("Client polling failed");
        return -1;
    }
    if(create_rsp_frame(data_size, &frame_size, buffer) < 0){
        return -1;
    }
    if(spi_transfer(frame_size) < 0){
        return -1;
    }

    if(frame_response_prefix[0] == buffer[1] && 
        frame_response_prefix[1] == buffer[2] &&
        frame_response_prefix[2] == buffer[3]){

        uint16_t checksum = *((uint16_t *) &buffer[frame_size - 2]);
        int response_payload_size = frame_size - FRAME_CHECKSUM_SIZE - CLIENT_RSP_PREFIX_SIZE;
        uint16_t calc_checksum = calculate_crc16(response_payload_size, &buffer[CLIENT_RSP_RSP_PAYLOAD_START]);
        if(checksum != calc_checksum){
            ERROR("SPI transport frame checksum mismatch");
            return -1;
        }
    memcpy(data, &buffer[CLIENT_RSP_RSP_PAYLOAD_START], response_payload_size);
    *size = response_payload_size;
    }
    return 0;
}

transport_t spi_transport ={
    .close = close,
    .open = open,
    .read = read,
    .write = write,
    .init = init
};

int get_spi_transport(transport_t **transport){
    *transport = &spi_transport;
    return 0;
}
