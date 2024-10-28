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
#include "mdfu_config.h"
#include "mdfu/mdfu.h"

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
int discard_until(uint8_t code, timeout_t timer)
{
    int status = -1;
    uint8_t data;

    while(true)
    {
        status = transport_mac.read(1, &data);
        assert(status <= 1);
        if(status < 0){
            break;
        }else if(status == 1) {
            if(data == code){
                status = 0;
                break;
            } 
        }
        if(timeout_expired(&timer)){
            DEBUG("Timeout expired while waiting for frame start code");
            status = -1;
            errno = ETIMEDOUT;
            break;
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
ssize_t read_until(uint8_t code, int max_size, uint8_t *data, timeout_t timer){
    int status = -1;
    uint8_t tmp;
    uint8_t *pdata = data;

    while(true)
    {
        if(max_size == pdata - data){
            errno = ENOBUFS;
            DEBUG("Buffer overflow in serial transport while waiting for frame end code");
            break;
        }
        status = transport_mac.read(1, &tmp);
        assert(status <= 1);

        if(status < 0){
            break;
        }else if(status == 1) {
            if(tmp == code){
                status = pdata - data;
                break;
            } else {
                *pdata = tmp;
                pdata++;
            }
        }
        if(timeout_expired(&timer)){
            status = -1;
            DEBUG("Timeout expired while waiting for frame end code");
            errno = ETIMEDOUT;
            break;
        }
    }
    return status;
}

/**
 * @brief Reads data from a transport layer and decodes it until the frame end code is received.
 *
 * This function reads bytes one by one from a transport layer, handling escape sequences
 * and stopping when the frame end code is encountered or the buffer is full. If an escape
 * sequence is detected, the function decodes it to the original byte. The function also
 * checks for a timeout condition.
 *
 * @param max_size The maximum number of bytes to read into the buffer.
 * @param data A pointer to the buffer where the decoded data will be stored.
 * @param timer A timeout_t structure that defines the timeout condition.
 *
 * @return On success, the number of bytes read and decoded. On failure, -1 and errno is set
 *         to indicate the error (ENOBUFS for buffer overflow, EINVAL for invalid escape sequence,
 *         ETIMEDOUT for timeout).
 *
 * @warning The function asserts that the status returned by transport_mac.read is less than or equal to 1.
 *          The transport_mac.read function is assumed to be a part of the transport_mac object which should
 *          be defined and initialized elsewhere.
 */
ssize_t read_and_decode_until(int max_size, uint8_t *data, timeout_t timer){
    int status = -1;
    uint8_t tmp;
    uint8_t *pdata = data;
    bool escape_code = false;

    while(true)
    {
        if(max_size == pdata - data){
            errno = ENOBUFS;
            DEBUG("Buffer overflow in serial transport while waiting for frame end code");
            break;
        }
        status = transport_mac.read(1, &tmp);
        assert(status <= 1);

        if(status < 0){
            break;
        }else if(status == 1) {
            if(tmp == FRAME_END_CODE){
                status = pdata - data;
                break;
            } 
            if(escape_code){
                escape_code = false;
                if(tmp == FRAME_START_ESC_SEQ){
                    *pdata++ = FRAME_START_CODE;
                } else if(tmp == FRAME_END_ESC_SEQ){
                    *pdata++ = FRAME_END_CODE;
                } else if(tmp == ESCAPE_SEQ_ESC_SEQ){
                    *pdata++ = ESCAPE_SEQ_CODE;
                } else {
                    DEBUG("Invalid code (%x) after escape code", tmp);
                    status = -1;
                    errno = EINVAL;
                    break;
                }
            } else {
                if(tmp == ESCAPE_SEQ_CODE){
                    escape_code = true;
                } else {
                    *pdata = tmp;
                    pdata++;
                }
            }
        }
        if(timeout_expired(&timer)){
            status = -1;
            DEBUG("Timeout expired while waiting for frame end code");
            errno = ETIMEDOUT;
            break;
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
    int status;
    uint16_t checksum;
    int decoded_size;
    timeout_t timer;
    set_timeout(&timer, timeout);

    status = discard_until(FRAME_START_CODE, timer);
    if(status < 0){
        return status;
    }
    status = read_until(FRAME_END_CODE, sizeof(buffer), buffer, timer);
    if(status < 0){
        return status;
    }
    *size = status;
    // decoding into the same buffer
    status = decode_frame_payload(*size, buffer, &decoded_size, data);
    if(status < 0){
        return status;
    }
    *size = decoded_size;
    uint16_t frame_checksum = *((uint16_t *) &data[*size - 2]);
    DEBUG("Got a frame: ");
    log_frame(*size, data);

    checksum = calculate_frame_checksum(*size - 2, data);
    if(checksum != frame_checksum){
        DEBUG("Serial Transport: Frame check sequence verification failed, calculated 0x%04x but got 0x%04x\n", checksum, frame_checksum);
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

    buffer[buf_index] = FRAME_END_CODE;
    buf_index += 1;
    DEBUG("Sending frame: ");
    log_frame(buf_index - 2, &buffer[1]);
    return transport_mac.write(buf_index, buffer);
}

transport_t serial_transport ={
    .close = close,
    .open = open,
    .read = read,
    .write = write,
    .init = init,
    .ioctl = NULL
};

int get_serial_transport(transport_t **transport){
    *transport = &serial_transport;
    return 0;
}
