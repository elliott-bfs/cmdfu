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
#include <stdarg.h>
#include "mdfu/transport/spi_transport.h"
#include "mdfu/timeout.h"
#include "mdfu/logging.h"
#include "mdfu/mdfu_config.h"
#include "mdfu/mdfu.h"
#include "mdfu/checksum.h"

/**
 * @brief MDFU SPI transport frame prefix to indicate a response length frame.
 */
static const char frame_length_prefix[] = {'L', 'E', 'N'};

/**
 * @brief MDFU SPI transport frame prefix to indicate a response frame.
 */
static const char frame_response_prefix[] = {'R', 'S', 'P'};

/**
 * This section defines various constants used for parsing client responses.
 *
 * CLIENT_RSP_PREFIX_SIZE: The size of the prefix in the client response frames.
 * CLIENT_RSP_LEN_LENGTH_SIZE: The size of the length field in the client response length frame.
 * CLIENT_RSP_LEN_LENGTH_START: The starting position of the length field in the client response length frame.
 * CLIENT_RSP_LEN_CHECKSUM_START: The starting position of the checksum field in the client response length frame.
 * CLIENT_RSP_RSP_PAYLOAD_START: The starting position of the payload in the client response frame.
 */
#define CLIENT_RSP_PREFIX_SIZE 4
#define CLIENT_RSP_LEN_LENGTH_SIZE 2
#define CLIENT_RSP_LEN_LENGTH_START 4
#define CLIENT_RSP_LEN_CHECKSUM_START 6
#define CLIENT_RSP_RSP_PAYLOAD_START 4

/**
 * @brief Constants for different frame types.
 *
 * FRAME_TYPE_CMD: Command frame type identifier.
 * FRAME_TYPE_RSP_RETRIEVAL: Response retrieval frame type identifier.
 * FRAME_TYPE_SIZE: Size of the frame type field in bytes.
 */
#define FRAME_TYPE_CMD 0x11
#define FRAME_TYPE_RSP_RETRIEVAL 0x55
#define FRAME_TYPE_SIZE 1

/**
 * @brief Size of the frame checksum in bytes.
 */
#define FRAME_CHECKSUM_SIZE 2

/**
 * @brief Defines the maximum size of the frame buffer.
 *
 * The frame buffer size is calculated as the sum of the frame type size,
 * the maximum size of the MDFU command packet, and the frame checksum size.
 */
#define FRAME_BUFFER_MAX_SIZE (FRAME_TYPE_SIZE + MDFU_CMD_PACKET_MAX_SIZE + FRAME_CHECKSUM_SIZE)

/** 
 * @brief MAC layer interface used for transport layer communication.
 * 
 * This variable holds the MAC layer interface.
 */
static mac_t *transport_mac = NULL;

/**
 * @brief SPI transport inter transaction delay timer.
 */
static timeout_t itd_timer;

/**
 * @brief Inter transaction delay in seconds.
 */
static float itd_delay = 0.01f;

/** 
 * @brief Buffer for storing SPI transport frames.
 * 
 * This buffer is used to store the frames that are constructed or received
 * by the transport layer. The size of the buffer is calculated based on the
 * maximum expected size of the frames, including frame type field,
 * maximum MDFU command packet length, and frame check sequence.
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
static int init(mac_t *mac, int timeout){ // NOSONAR
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
 * @brief Logs a frame if the debug level is appropriate.
 *
 * This function logs the size and payload of a frame. It first checks if the
 * current debug level is greater than the specified debug level. If it is, the function
 * returns without logging anything. Otherwise, it logs the size of the data frame and
 * the payload in hexadecimal format.
 *
 * @param size The size of the frame.
 * @param data A pointer to the frame to be logged.
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
 * @brief Creates a SPI transport command frame.
 *
 * @param size The size of the MDFU packet to be included in the frame.
 * @param data Pointer to the MDFU packet to be included in the frame.
 * @param frame_size Pointer to an integer where the size of the created frame will be stored.
 * @param frame Pointer to the buffer where the created frame will be stored.
 * @return 0 on success, -1 on error with errno set appropriately.
 *
 * This function constructs a command frame by adding a frame type, copying the data,
 * and appending a CRC16 checksum. If the size of the data exceeds the buffer capacity,
 * it sets errno to EOVERFLOW and returns -1.
 */
static int create_cmd_frame(int size, uint8_t *data, int *frame_size, uint8_t *frame){
    int buf_index = 0;
    uint16_t frame_check_sequence;

    if(size > (sizeof(buffer) - FRAME_CHECKSUM_SIZE - FRAME_TYPE_SIZE)){
        errno = EOVERFLOW;
        return -1;
    }

    frame[buf_index++] = FRAME_TYPE_CMD;

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
 * Creates a response frame for SPI transport.
 *
 * This function constructs a response frame with a specified response length.
 * It ensures that the buffer can hold the full response, including the 2-byte CRC.
 * If the buffer is too small, it sets the errno to EOVERFLOW and returns -1.
 * Otherwise, it fills the frame with the appropriate data and zeroes out the remaining bytes
 * for better debugging.
 *
 * @param response_length The length of the response, including the 2-byte CRC.
 * @param frame_size Pointer to an integer where the size of the created frame will be stored.
 * @param frame Pointer to the buffer where the frame will be created.
 * @return 0 on success, -1 on failure with errno set to EOVERFLOW.
 */
static int create_rsp_frame(int response_length, int *frame_size, uint8_t *frame){
    int buf_index = 0;

    // Ensure the buffer can hold a full response
    // The response length includes the 2-bytes CRC
    if((CLIENT_RSP_PREFIX_SIZE + response_length) > sizeof(buffer)){
        errno = EOVERFLOW;
        ERROR("SPI transport buffer to small to fit command");
        return -1;
    }
    frame[buf_index++] = FRAME_TYPE_RSP_RETRIEVAL;
    // Zero out remaining frame bytes. These are don't care but its for
    // better debugging etc. The number of zero bytes is the payload plus
    // the prefix minus one, since the first byte contains the
    // frame type
    for(int i = 0; i < (CLIENT_RSP_PREFIX_SIZE - FRAME_TYPE_SIZE + response_length); i++){
        frame[buf_index++] = 0x00;
    }
    *frame_size = buf_index;
    return 0;
}

/**
 * Transfers data over SPI.
 *
 * This function handles the SPI data transfer by first waiting for the inter-transaction delay
 * timeout to expire. It then sends a frame of the specified size and logs the frame. If the
 * write operation fails, it resets the timeout and returns an error. After setting the timeout
 * for the inter-transaction delay, it reads the data back. If the read operation fails or the
 * read size does not match the write size, it returns an error. Otherwise, it logs the received
 * frame and returns success.
 *
 * @param size The size of the data frame to be transferred.
 * @return 0 on success, -1 on failure.
 */
static int spi_transfer(int size){
    int read_size;

    // wait for inter transaction delay timeout to expire
    while(!timeout_expired(&itd_timer)){/* do nothing  */}

    TRACE(DEBUGLEVEL, "DEBUG:SPI transport sending frame: ");
    log_frame(size, buffer);
    if(transport_mac->write(size, buffer) < 0){
        set_timeout(&itd_timer, itd_delay);
        return -1;
    }
    if(set_timeout(&itd_timer, itd_delay) < 0){
        return -1;
    }
    // No need to have a inter transaction timeout on read
    // because the write implicitely did already the read.
    read_size = transport_mac->read(size, buffer);
    if(read_size < 0){

        return -1;
    }

    TRACE(DEBUGLEVEL, "DEBUG:SPI transport received frame: ");
    log_frame(read_size, buffer);
    if(read_size != size){
        ERROR("SPI MAC layer read size did not match write size");
        return -1;
    }
    return 0;
}

/**
 * @brief Sends a MDFU packet over SPI transport.
 *
 * This function creates a SPI transport command frame from the provided data and size,
 * and then transfers the frame over SPI.
 *
 * @param size The size of the MDFU packet to be written.
 * @param data A pointer to the MDFU packet to be written.
 * @return int Returns -1 if creating the command frame fails, otherwise returns the result of spi_transfer.
 */
static int write(int size, uint8_t *data){
    int frame_size = 0;
    
    if(create_cmd_frame(size, data, &frame_size, buffer) < 0){
        return -1;
    }
    return spi_transfer(frame_size);
}

/**
 * Polls for a client response length within a specified timeout period.
 *
 * This function continuously polls for a client response length by creating a response frame
 * and transferring it via SPI. It checks for a valid frame length prefix and verifies
 * the response length and checksum. If the response length is less than 2 bytes or if
 * there is a checksum mismatch, an error is logged and the function returns -1.
 * If the timeout expires during polling, the function also returns -1.
 *
 * @param timer A pointer to a timeout_t structure that specifies the timeout period.
 * @return The length of the client response if successful, or -1 if an error occurs.
 */
static ssize_t poll_for_client_response_length(timeout_t *timer){
    int frame_size;
    int response_length = -1;

    // Poll for a client response
    while(true){
        if(create_rsp_frame(CLIENT_RSP_LEN_LENGTH_SIZE + FRAME_CHECKSUM_SIZE, &frame_size, buffer) < 0){
            return -1;
        }
        if(spi_transfer(frame_size) < 0){
            return -1;
        }

        if(frame_length_prefix[0] == buffer[1] &&
            frame_length_prefix[1] == buffer[2] &&
            frame_length_prefix[2] == buffer[3]){

            response_length = *((uint16_t *) &buffer[CLIENT_RSP_LEN_LENGTH_START]);
            if(response_length < 2){
                ERROR("SPI transport response length must be at lest 2 bytes but client reported %d", response_length);
                return -1;
            }
            uint16_t checksum = *((uint16_t *) &buffer[CLIENT_RSP_LEN_CHECKSUM_START]);
            uint16_t calc_checksum = calculate_crc16(CLIENT_RSP_LEN_LENGTH_SIZE, &buffer[CLIENT_RSP_LEN_LENGTH_START]);
            if(checksum != calc_checksum){
                ERROR("SPI transport frame checksum mismatch");
                return -1;
            }
            break;
        }
        DEBUG("Received client busy frame");
        if(timeout_expired(timer)){
            DEBUG("Timeout during polling for response length");
            return -1;
        }
    }
    return response_length;
}

/**
 * Polls for a client response within a specified timeout period.
 *
 * This function continuously polls for a client response by transferring data over SPI.
 * It verifies the response frame's prefix, size, and checksum to ensure data integrity.
 * If the response is valid, it copies the response payload to the provided data buffer.
 * The function returns 0 on success and -1 on failure.
 *
 * @param timer Pointer to a timeout structure that defines the polling timeout period.
 * @param response_length Expected length of the response frame.
 * @param data Pointer to a buffer where the response payload will be copied.
 * @return 0 on success, -1 on failure.
 */
static int poll_for_client_response(timeout_t *timer, int response_length, uint8_t *data){
    int frame_size;

    if(create_rsp_frame(response_length, &frame_size, buffer) < 0){
        return -1;
    }
    while(true){

        if(spi_transfer(frame_size) < 0){
            return -1;
        }

        if(frame_response_prefix[0] == buffer[1] &&
            frame_response_prefix[1] == buffer[2] &&
            frame_response_prefix[2] == buffer[3]){

            if (frame_size < 2) {
                ERROR("SPI transport frame size is too small");
                return -1;
            }
            uint16_t checksum = *((uint16_t *) &buffer[frame_size - 2]);
            int response_payload_size = frame_size - FRAME_CHECKSUM_SIZE - CLIENT_RSP_PREFIX_SIZE;
            if(response_payload_size > MDFU_RESPONSE_PACKET_MAX_SIZE){
                ERROR("SPI transport response length (%d) exceeds maximum MDFU response packet size (%d)", response_payload_size, MDFU_CMD_PACKET_MAX_SIZE);
                return -1;
            }
            uint16_t calc_checksum = calculate_crc16(response_payload_size, &buffer[CLIENT_RSP_RSP_PAYLOAD_START]);
            if(checksum != calc_checksum){
                ERROR("SPI transport frame checksum mismatch");
                return -1;
            }
            memcpy(data, &buffer[CLIENT_RSP_RSP_PAYLOAD_START], response_payload_size);
            break;
        }
        DEBUG("Received client busy frame");
        if(timeout_expired(timer)){
            DEBUG("Timeout during polling for response length");
            return -1;
        }
    }
    return 0;
}

/**
 * @brief Reads a MDFU response from client with a specified timeout.
 *
 * This function initiates a polling process to read the response length from a client
 * and then reads the actual response if the length is valid. The size of the
 * response data is returned through the size parameter.
 *
 * @param size Pointer to an integer where the size of the response will be stored.
 * @param data Pointer to a buffer where the response will be stored.
 * @param timeout The maximum time to wait for the client response, in seconds.
 *
 * @return 0 on success, -1 on failure (e.g., if the response length is less than 2 or
 *         if polling for the client response fails).
 */
static int read(int *size, uint8_t *data, float timeout){
    timeout_t timer;
    ssize_t response_length;

    DEBUG("Starting client response length polling");
    set_timeout(&timer, timeout);
    response_length = poll_for_client_response_length(&timer);
    if(response_length < 2){
        return -1;
    }
    DEBUG("Starting client response polling");
    if(poll_for_client_response(&timer, (int) response_length, data) < 0){
        return -1;
    }
    *size = (int) response_length - FRAME_CHECKSUM_SIZE;
    return 0;
}

/**
 * @brief Handle ioctl requests for transport settings.
 *
 * This function processes various ioctl requests to configure transport settings.
 * It currently supports the following request:
 * - TRANSPORT_IOC_INTER_TRANSACTION_DELAY: Sets the inter-transaction delay.
 *
 * @param request The ioctl request code.
 * @param ... Additional arguments depending on the request code.
 * @return 0 on success, -1 on failure.
 */
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

/**
 * @brief Structure representing SPI transport functions.
 *
 * This structure contains function pointers for various SPI transport operations
 * such as open, close, read, write, init, and ioctl.
 */
transport_t spi_transport ={
    .close = close,
    .open = open,
    .read = read,
    .write = write,
    .init = init,
    .ioctl = ioctl
};

/**
 * @brief Retrieves the SPI transport instance.
 *
 * This function assigns the address of the SPI transport instance to the provided
 * transport pointer and returns 0 to indicate success.
 *
 * @param[out] transport Pointer to a transport_t pointer that will be set to the address
 *                       of the SPI transport instance.
 * @return int Returns 0 to indicate success.
 */
int get_spi_transport(transport_t **transport){
    *transport = &spi_transport;
    return 0;
}
