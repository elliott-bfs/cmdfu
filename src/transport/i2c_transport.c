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
#include "mdfu/mdfu_config.h"
#include "mdfu/mdfu.h"
#include "mdfu/checksum.h"


/**
 * @brief Constant representing the response length frame type.
 */
static const char rsp_frame_type_length = 'L';

/**
 * @brief Constant representing the response frame type.
 */
static const char rsp_frame_type_response = 'R';


/**
 * @brief Defines the size of the frame type.
 */
#define FRAME_TYPE_SIZE 1
/**
 * @brief Size of the frame checksum in bytes.
 */
#define FRAME_CHECKSUM_SIZE 2
/**
 * @brief Defines the size of the response length frame.
 */
#define RSP_LENGTH_FRAME_SIZE 5
/**
 * @brief Defines the starting position of the frame length in the response.
 */
#define RSP_LENGTH_FRAME_LENGTH_START 1
/**
 * @brief Defines the starting position of the CRC in the response frame.
 */
#define RSP_LENGTH_FRAME_CRC_START 3
/**
 * @brief Length of the response frame length field in bytes.
 */
#define RSP_LENGTH_FRAME_LENGTH_SIZE 2

/**
 * @brief Defines the maximum size of the frame buffer.
 *
 * This macro calculates the maximum size of the frame buffer by summing the sizes of the frame type,
 * MDFU command packet, and frame checksum. The resulting value represents the total maximum size
 * that the frame buffer can accommodate.
 */
#define FRAME_BUFFER_MAX_SIZE (FRAME_TYPE_SIZE + MDFU_CMD_PACKET_MAX_SIZE + FRAME_CHECKSUM_SIZE)

/** 
 * @brief MAC layer interface used for transport layer communication.
 * 
 * This variable holds the MAC layer interface.
 */
static mac_t *transport_mac;

/**
 * @brief I2C transport inter transaction delay timer.
 */
static timeout_t itd_timer;

/**
 * @brief Inter transaction delay in seconds.
 */
static float itd_delay = 0.01f;

/** 
 * @brief Buffer for storing data frames.
 * 
 * This buffer is used to store the data frames that are constructed or received
 * by the transport layer. The size of the buffer is calculated based on the
 * maximum expected size of the frames, including frame type field, sequence field,
 * command size, maximum command data length, and frame check sequence.
 */
static uint8_t buffer[FRAME_BUFFER_MAX_SIZE];

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

/**
 * @brief Logs transport frame at a specified debug level.
 *
 * This function logs the size and payload of a frame if the current
 * debug level is less than or equal to the specified DEBUGLEVEL.
 *
 * @param size The size of the frame.
 * @param data Pointer to the frame to be logged.
 */
static void log_frame(int size, const uint8_t *data){
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


/**
 * Creates a command frame.
 *
 * @param size The size of the MDFU packet.
 * @param data Pointer to the MDFU packet buffer.
 * @param frame_size Pointer to an integer where the size of the created frame will be stored.
 * @param frame Pointer to the buffer where the created frame will be stored.
 * @return 0 on success, -1 on error (with errno set to EOVERFLOW if the input size is too large).
 */
static int create_cmd_frame(int size, uint8_t *data, int *frame_size, uint8_t *frame){
    int buf_index = 0;
    uint16_t frame_check_sequence;

    if((size + FRAME_CHECKSUM_SIZE) > sizeof(buffer)){
        errno = EOVERFLOW;
        return -1;
    }

    for(int i = 0;i < size; i++){
        frame[buf_index++] = data[i];
    }
    frame_check_sequence = calculate_crc16(size, data);
    frame[buf_index++] = (uint8_t) (frame_check_sequence & 0xff);
    frame[buf_index++] = (uint8_t) ((frame_check_sequence >> 8) & 0xff);
    *frame_size = buf_index;
    return 0;
}

/**
 * @brief Sends a MDFU command via I2C transport.
 *
 * This function creates a MDFU command packet by using the I2C
 * transport mechanism.
 * It logs the frame before sending and handles any errors that
 * occur during the write operation as per the MDFU specification.
 * The function also sets a timeout for the operation.
 *
 * @param size The size of the MDFU packet to be written.
 * @param data Pointer to the MDFU packet to be written.
 * @return int Status of the write operation. Returns 0 on success, 
 *         -1 on failure.
 */
static int write(int size, uint8_t *data){
    int frame_size = 0;
    int status = 0;
    
    if(create_cmd_frame(size, data, &frame_size, buffer) < 0){
        return -1;
    }

    TRACE(DEBUGLEVEL, "DEBUG:I2C transport sending frame: ");
    log_frame(frame_size, buffer);

    while(!timeout_expired(&itd_timer)){/* do nothing*/}

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
 * Polls for a client response length within a specified timeout period.
 *
 * This function continuously polls for a client response length frame using the I2C transport
 * mechanism. It waits for a response until the specified timeout expires. If a valid response
 * length frame is received, it verifies the checksum and returns the data size. If the checksum
 * does not match, it returns a checksum error. If the timeout expires during polling, it returns
 * a timeout error.
 *
 * @param timer Pointer to the timeout structure.
 * @return The size of the data if a valid response length frame is received.
 *         -TIMEOUT_ERROR if the timeout expires during polling.
 *         -CHECKSUM_ERROR if the checksum verification fails.
 *         -1 for other errors.
 */
static ssize_t poll_for_client_response_length(timeout_t *timer){
    int data_size = -1;

    // Poll for a client response
    while(true){
        while(!timeout_expired(&itd_timer)){/* do nothing*/}

        DEBUG("Polling client for response length");
        if(transport_mac->read(RSP_LENGTH_FRAME_SIZE, buffer) < 0){
            set_timeout(&itd_timer, itd_delay);
            if(timeout_expired(timer)){
                DEBUG("Timeout during polling for response length");
                return -TIMEOUT_ERROR;
            }
            continue;
        }
        if(0 > set_timeout(&itd_timer, itd_delay)){
            return -1;
        }
        TRACE(DEBUGLEVEL, "DEBUG:I2C transport received frame: ");
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
 * Polls for a client response within a specified timeout period.
 *
 * This function continuously polls for a client response until either a valid response is received
 * or the specified timeout period expires. It reads the response frame from the transport layer,
 * verifies the frame type, and checks the checksum for data integrity. If the response is valid,
 * it copies the response data to the provided buffer.
 *
 * @param timer Pointer to the timeout structure.
 * @param response_length Length of the expected response data.
 * @param data Pointer to the buffer where the response will be copied.
 * @return 0 on success, or a negative error code on failure:
 *         -EOVERFLOW if the response frame length exceeds the allocated buffer size.
 *         -TIMEOUT_ERROR if the polling times out.
 *         -CHECKSUM_ERROR if there is a checksum mismatch.
 *         -EINVAL if the response_length is invalid
 */
static int poll_for_client_response(timeout_t *timer, int response_length, uint8_t *data){
    if(FRAME_TYPE_SIZE + response_length > sizeof(buffer)){
        ERROR("I2C transport response frame length (%d) exceeds allocated buffer (%d)", FRAME_TYPE_SIZE + response_length, (int) sizeof(buffer));
        return -EOVERFLOW;
    }
    if(response_length < 2){
        ERROR("I2C transport: Invalid response length (%d). Expected at least a length of 2.", response_length);
        return -EINVAL;
    }
    while(true){
        while(!timeout_expired(&itd_timer)){/* do nothing*/}

        if(transport_mac->read(FRAME_TYPE_SIZE + response_length, buffer) < 0){
            set_timeout(&itd_timer, itd_delay);
            if(timeout_expired(timer)){
                DEBUG("Timeout during polling for response");
                return -TIMEOUT_ERROR;
            }
            continue;
        }
        set_timeout(&itd_timer, itd_delay);

        TRACE(DEBUGLEVEL, "DEBUG:I2C transport received response frame: ");
        log_frame(FRAME_TYPE_SIZE + response_length, buffer);

        if(rsp_frame_type_response == buffer[0]){

            uint16_t checksum = *((uint16_t *) &buffer[FRAME_TYPE_SIZE + response_length - FRAME_CHECKSUM_SIZE]);
            uint16_t calc_checksum = calculate_crc16(response_length - FRAME_CHECKSUM_SIZE, &buffer[FRAME_TYPE_SIZE]);
            if(checksum != calc_checksum){
                ERROR("I2C transport frame checksum mismatch");
                return -CHECKSUM_ERROR;
            }
            if((response_length - FRAME_CHECKSUM_SIZE) > MDFU_RESPONSE_PACKET_MAX_SIZE){
                ERROR("Received MDFU response packet (%d) exceeds allocated buffer (%d)", response_length - FRAME_CHECKSUM_SIZE, MDFU_RESPONSE_PACKET_MAX_SIZE);
                return -EOVERFLOW;
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
 * @brief Reads MDFU response from a client.
 *
 * This function polls for a client response length and then reads the client response.
 * It uses a timeout mechanism to ensure the operations do not hang indefinitely.
 *
 * @param size Pointer to an integer where the size of the MDFU response packet will be stored.
 * @param data Pointer to a buffer where the MDFU response packet will be stored.
 * @param timeout The maximum time to wait for the client response, in seconds.
 *
 * @return 0 on success, or a negative value on error.
 */
static int read(int *size, uint8_t *data, float timeout){
    timeout_t timer;
    ssize_t response_length;
    int status;

    // Poll for a client response
    DEBUG("Starting client response length polling");
    set_timeout(&timer, timeout);
    response_length = poll_for_client_response_length(&timer);
    if(response_length < 0){
        return (int) response_length;
    }
    *size = (int) (response_length - FRAME_CHECKSUM_SIZE);
    DEBUG("Starting client response polling");
    status = poll_for_client_response(&timer, (int) response_length, data);
    if(status < 0){
        return status;
    }
    return 0;
}


static int ioctl(int request, ...){
    va_list args;
    va_start(args, request);
    int result = -1;
    if(TRANSPORT_IOC_INTER_TRANSACTION_DELAY == request){
        itd_delay = (float) va_arg(args, double);
        result = 0;
    }
    va_end(args);
    return result;
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