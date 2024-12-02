/**
 * @file serial_transport.c
 * @brief Serial transport layer.
 */
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <errno.h>
#include <assert.h>
#include "mdfu/transport/serial_transport.h"
#include "mdfu/timeout.h"
#include "mdfu/logging.h"
#include "mdfu/mdfu_config.h"
#include "mdfu/mdfu.h"
#include "mdfu/checksum.h"

/** @def FRAME_CHECK_SEQUENCE_SIZE
 *  @brief Size of the frame check sequence in bytes.
 */
#define FRAME_CHECK_SEQUENCE_SIZE 2

/** @def FRAME_PAYLOAD_LENGTH_SIZE
 *  @brief Size of payload length field in bytes.
 */
#define FRAME_PAYLOAD_LENGTH_SIZE 2

/** @def FRAME_START_CODE_SIZE
 *  @brief Size of the frame start code in bytes.
 */
#define FRAME_START_CODE_SIZE 1

/** @def FRAME_END_CODE_SIZE
 *  @brief Size of the frame end code in bytes.
 */
#define FRAME_END_CODE_SIZE 1

/** @def FRAME_START_CODE
 *  @brief Indicates the start of a frame.
 */
#define FRAME_START_CODE 0x56

/** @def FRAME_END_CODE
 *  @brief The ending byte code of a frame.
 */
#define FRAME_END_CODE 0x9E

/** @def ESCAPE_SEQ_CODE
 *  @brief The byte code used to indicate the start of an escape sequence.
 */
#define ESCAPE_SEQ_CODE 0xCC

/** @def FRAME_START_ESC_SEQ
 *  @brief The escape sequence replacement for the frame start code.
 */
#define FRAME_START_ESC_SEQ (~FRAME_START_CODE & 0xff)

/** @def FRAME_END_ESC_SEQ
 *  @brief The escape sequence replacement for the frame end code.
 */
#define FRAME_END_ESC_SEQ  (~FRAME_END_CODE & 0xff)

/** @def ESCAPE_SEQ_ESC_SEQ
 *  @brief The escape sequence replacement for the escape sequence code itself.
 */
#define ESCAPE_SEQ_ESC_SEQ (~ESCAPE_SEQ_CODE & 0xff)


/** 
 * @brief MAC layer interface used for transport layer communication.
 * 
 * This variable holds the MAC layer interface.
 */
static mac_t transport_mac;

/** 
 * @brief Buffer for storing data frames.
 * 
 * This buffer is used to store the data frames that are constructed or received
 * by the transport layer. The size of the buffer is calculated based on the
 * maximum expected size of the frames, including start/end codes, sequence field,
 * command size, maximum command data length, and frame check sequence.
 * 
 * @note The buffer size is calculated for the worst case scenario where all data
 * bytes consist of reserved codes that need to be replaced with escape sequences.
 */
static uint8_t buffer[FRAME_START_CODE + (MDFU_SEQUENCE_FIELD_SIZE + MDFU_COMMAND_SIZE  + MDFU_MAX_COMMAND_DATA_LENGTH + FRAME_CHECK_SEQUENCE_SIZE) * 2 + FRAME_END_CODE_SIZE];

/**
 * @brief Discards all incoming data until a specific code is encountered or a timeout occurs.
 *
 * This function reads data from a transport medium character by character until it finds the specified code.
 * If the code is found, the function returns 0. If a read error occurs or a timeout expires before finding the code,
 * the function returns -1 and sets the errno to ETIMEDOUT in case of a timeout.
 *
 * @param code The byte code to look for in the incoming data stream.
 * @param timer A timeout_t structure that defines the timeout condition.
 *
 * @return int Returns 0 if the code is found, -1 if an error occurs or if a timeout expires.
 *
 */
static int discard_until(uint8_t code, timeout_t timer)
{
    int status = -1;
    uint8_t data;
    bool continue_discarding = true;

    while(continue_discarding)
    {
        status = transport_mac.read(1, &data);
        assert(status <= 1);
        if(status < 0){
            continue_discarding = false;
        }else if((status == 1) && (data == code)){
                status = 0;
                continue_discarding = false;
        }
        if(continue_discarding && timeout_expired(&timer)){
            DEBUG("Timeout expired while waiting for frame start code");
            status = -1;
            errno = ETIMEDOUT;
            continue_discarding = false;
        }
    }
    return status;
}

/**
 * @brief Reads data from MAC layer until a specified code is encountered or a maximum size is reached.
 *
 * This function reads bytes one by one from a MAC layer into a buffer until either the specified end code is read,
 * the buffer is filled to its maximum size, or a timeout occurs.
 *
 * @param code The byte code to read until.
 * @param max_size The maximum number of bytes to read into the buffer.
 * @param data A pointer to the buffer where the read bytes will be stored.
 * @param timer A timeout structure that determines how long to wait for the end code before giving up.
 *
 * @return On success, the number of bytes read into the buffer (not including the end code).
 *         On failure, -1 is returned and errno is set appropriately:
 *         - ENOBUFS if the buffer is too small for the incoming data.
 *         - ETIMEDOUT if the operation times out before the end code is encountered.
 *         - Error codes from the MAC layer are passed through
 */
static ssize_t read_until(uint8_t code, int max_size, uint8_t *data, timeout_t timer){
    int status = -1;
    uint8_t tmp;
    uint8_t *pdata = data;
    bool continue_reading = true;

    while(continue_reading)
    {
        if(max_size == pdata - data){
            errno = ENOBUFS;
            DEBUG("Buffer overflow in serial transport while waiting for frame end code");
            continue_reading = false;
        }else{
            status = transport_mac.read(1, &tmp);
            assert(status <= 1);

            if(status < 0){
                continue_reading = false;
            }else if(status == 1 ) {
                if(tmp == code){
                    status = (int) (pdata - data);
                    continue_reading = false;
                } else {
                    *pdata = tmp;
                    pdata++;
                }
            }
            if(continue_reading && timeout_expired(&timer)){
                status = -1;
                DEBUG("Timeout expired while waiting for frame end code");
                errno = ETIMEDOUT;
                continue_reading = false;
            }
        }
    }
    return status;
}

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
static int init(mac_t *mac, int timeout){//NOSONAR
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

/**
 * @brief Encodes the payload of a frame by escaping special codes.
 *
 * This function takes an array of data and encodes it by inserting escape sequences
 * before any special codes that appear in the data. The special codes include the
 * start of frame code, end of frame code, and the escape sequence code itself.
 *
 * @param data_size The size of the input data array.
 * @param data A pointer to the array of data that needs to be encoded.
 * @param encoded_data A pointer to the array where the encoded data will be stored.
 * @param encoded_data_size A pointer to an integer where the size of the encoded data will be stored.
 *
 * @note The encoded_data array should be large enough to accommodate the encoded data,
 *       which may be larger than the input data due to the addition of escape sequences.
 *
 * @warning The function assumes that the encoded_data array has enough space to hold
 *          the encoded data. If the encoded_data array is not large enough, a buffer
 *          overflow can occur.
 */
static void encode_frame_payload(int data_size, const uint8_t *data, uint8_t *encoded_data, int *encoded_data_size){
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

static int decode_frame_payload(int data_size, const uint8_t *data, int *decoded_data_size, uint8_t *decoded_data){
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
                DEBUG("Invalid code (%x) after escape code", code);
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
    if(DEBUGLEVEL > debug_level){
        return;
    }
    TRACE(DEBUGLEVEL, "size=%d payload=0x", size);
    
    for(; i < size - 2; i++){
        TRACE(DEBUGLEVEL, "%02x", data[i]);
    }
    TRACE(DEBUGLEVEL, " fcs=0x%04x\n", *((uint16_t *) &data[size - 2]));
}

static int read(int *size, uint8_t *data, float timeout){
    ssize_t status;
    uint16_t checksum;
    int decoded_size;
    timeout_t timer;
    set_timeout(&timer, timeout);

    status = discard_until(FRAME_START_CODE, timer);
    if(status < 0){
        goto exit;
    }

    status = read_until(FRAME_END_CODE, sizeof(buffer), buffer, timer);
    if(status < 0){
        goto exit;
    }
    *size = (int) status;

    status = decode_frame_payload(*size, buffer, &decoded_size, data);
    if(status < 0){
        goto exit;
    }
    *size = decoded_size;
    uint16_t frame_checksum = *((uint16_t *) &data[*size - 2]);
    DEBUG("Got a frame: ");
    log_frame(*size, data);

    checksum = calculate_crc16(*size - 2, data);
    if(checksum != frame_checksum){
        DEBUG("Serial Transport: Frame check sequence verification failed, calculated 0x%04x but got 0x%04x\n", checksum, frame_checksum);
        status = -1;
        goto exit;
    }
    *size -= 2; // remove checksum size to get payload size
    exit:
        return (int) status;
}

/**
 * @brief Writes a MDFU packet to a serial transport.
 *
 * This function writes a MDFU packet to a serial transport. It calculates the checksum for the
 * frame, sends the frame start code, encodes and sends the MDFU packet, sends the checksum, and
 * finally sends the frame end code.
 *
 * @param size The size of the MDFU packet to be sent.
 * @param data Pointer to the buffer containing the MDFU packet to be sent.
 *
 * @return 0 on success, negative value on error with errno set appropriately.
 */
static int write(int size, uint8_t *data){
    int encoded_data_size = 0;
    int buf_index = 0;

    buffer[buf_index] = FRAME_START_CODE;
    buf_index += 1;

    uint16_t frame_check_sequence = calculate_crc16(size, data);
    encode_frame_payload(size, data, &buffer[buf_index], &encoded_data_size);
    buf_index += encoded_data_size;
    encode_frame_payload(2, (uint8_t *) &frame_check_sequence, &buffer[buf_index], &encoded_data_size);
    buf_index += encoded_data_size;

    buffer[buf_index] = FRAME_END_CODE;
    buf_index += 1;
    DEBUG("Sending frame: ");
    log_frame(buf_index - 2, &buffer[1]);
    return transport_mac.write(buf_index, buffer);
}

transport_t serial_transport_buffered ={
    .close = close,
    .open = open,
    .read = read,
    .write = write,
    .init = init,
    .ioctl = NULL
};

int get_serial_transport_buffered(transport_t **transport){
    *transport = &serial_transport_buffered;
    return 0;
}
