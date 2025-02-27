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
#include "mdfu/error.h"

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
        // if we have an error e.g. buffer overrun or framing error
        // keep going to dispose of any characters until we time out
        // so that we can have a fresh start on the next attempt
        if(status < 0){
            continue_discarding = true;
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
 * @brief Processes a byte of data, handling escape sequences if necessary.
 *
 * This function processes a single byte of data, checking if it is part of an escape sequence.
 * If an escape sequence is detected, it translates the sequence into the appropriate byte.
 * Otherwise, it stores the byte directly. The function updates the data pointer and escape code
 * status as needed.
 *
 * @param tmp The byte to be processed.
 * @param pdata Pointer to the data buffer pointer where the processed byte will be stored.
 * @param escape_code Pointer to a boolean flag indicating if the previous byte was an escape code.
 *
 * @return 0 on success, -1 on error with errno set to EINVAL if an invalid escape sequence is encountered.
 *
 * @note The function assumes that the caller has allocated sufficient space in the data buffer
 *       pointed to by pdata to store the processed byte.
 */
static int process_byte(uint8_t tmp, uint8_t **pdata, bool *escape_code)
{
    if (*escape_code){
        *escape_code = false;
        if (tmp == FRAME_START_ESC_SEQ){
            **pdata = FRAME_START_CODE;
        }else if (tmp == FRAME_END_ESC_SEQ){
            **pdata = FRAME_END_CODE;
    	}else if (tmp == ESCAPE_SEQ_ESC_SEQ){
            **pdata = ESCAPE_SEQ_CODE;
        }else{
            DEBUG("Invalid code (%x) after escape code", tmp);
            errno = EINVAL;
            return -1;
        }
        (*pdata)++;
    }else{
        if (tmp == ESCAPE_SEQ_CODE){
            *escape_code = true;
        }else{
            **pdata = tmp;
            (*pdata)++;
        }
    }
    return 0;
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
static ssize_t read_and_decode_until(int max_size, uint8_t *data, timeout_t timer){
    int status = -1;
    uint8_t tmp;
    uint8_t *pdata = data;
    bool escape_code = false;
    bool continue_reading = true;

    DEBUG("Receiving frame: ");
    while(continue_reading)
    {
        if(max_size == pdata - data){
            errno = ENOBUFS;
            DEBUG("Buffer overflow in serial transport while waiting for frame end code");
            break;
        }
        status = transport_mac.read(1, &tmp);
        assert(status <= 1);

        if(status < 0){
            continue_reading = false;
        }else if(status == 1) {
            if(tmp == FRAME_END_CODE){
                status = (int) (pdata - data);
                continue_reading = false;
            }else{
                status = process_byte(tmp, &pdata, &escape_code);
                if(status < 0){
                    continue_reading = false;
                }
            }
        }
        if(continue_reading && timeout_expired(&timer)){
            status = -1;
            DEBUG("Timeout expired while waiting for frame end code");
            errno = ETIMEDOUT;
            continue_reading = false;
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
static int init(mac_t *mac, int timeout){ // cppcheck-suppress
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
 * @brief Encodes data and sends it to MDFU client
 *
 * This function takes an array of data and encodes it by inserting escape sequences
 * before any special codes that appear in the data, and then sends the data
 * via the MAC layer to the MDFU client.
 *
 * @param data_size The size of the input data array.
 * @param data A pointer to the array of data that needs to be encoded.
 *
 */
static int encode_and_send(int data_size, const uint8_t *data){
    uint8_t code;
    uint8_t encoded_data[2];
    int size = 0;
    int status = 0;

    for(int i = 0; i < data_size; i++)
    {
        size = 0;
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
        if(transport_mac.write(size, (uint8_t *) &encoded_data) < 0){
            status = -1;
            break;
        }
    }
    return status;
}

static void log_frame(int size, const uint8_t *data, uint16_t checksum){
    int i = 0;
    if(DEBUGLEVEL > debug_level){
        return;
    }
    TRACE(DEBUGLEVEL, "size=%d payload=0x", size);
    
    for(; i < size - 2; i++){
        TRACE(DEBUGLEVEL, "%02x", data[i]);
    }
    TRACE(DEBUGLEVEL, " fcs=0x%04x\n", checksum);
}

/**
 * @brief Reads and decodes a MDFU packet from a serial transport.
 *
 * This function reads a MDFU response packet from a serial transport, discarding any bytes until the
 * start of the packet is detected. It then reads and decodes the packet, verifies the checksum,
 * and returns the size of the packet.
 *
 * @param size Pointer to an integer where the size of the MDFU packet will be stored.
 * @param data Pointer to a buffer where the decoded MDFU packet will be stored.
 * @param timeout The timeout period for reading the packet, in seconds.
 *
 * @return 0 on success, -1 on error with errno set appropriately.
 *
 * @note The function assumes that the caller has allocated sufficient space in the data buffer
 *       pointed to by data to store the decoded packet.
 */
static int read(int *size, uint8_t *data, float timeout){
    ssize_t status;
    uint16_t checksum;
    timeout_t timer;
    set_timeout(&timer, timeout);

    status = discard_until(FRAME_START_CODE, timer);
    if(status < 0){
        return (int) status;
    }
    status = read_and_decode_until(MDFU_CMD_PACKET_MAX_SIZE, data, timer);
    if(status < 0){
        return (int) status;
    }
    *size = (int) status;
    // Minimum status response should be 1 byte status and two bytes for CRC
    if(*size < 3){
        DEBUG("Serial Transport: Received invalid frame with length %d but minimum is 3", *size);
        return -1;
    }
    uint16_t frame_checksum = *((uint16_t *) &data[*size - 2]);
    log_frame(*size, data, frame_checksum);
    checksum = calculate_crc16(*size - 2, data);
    if(checksum != frame_checksum){
        DEBUG("Serial Transport: Frame check sequence verification failed, calculated 0x%04x but got 0x%04x\n", checksum, frame_checksum);
        return -1;
    }
    *size -= 2; // remove checksum size to get payload size
    return 0;
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
    int status = 0;
    uint8_t code;
    uint16_t frame_check_sequence = calculate_crc16(size, data);
    
    // Send frame start code
    code = FRAME_START_CODE;
    status = transport_mac.write(1, &code);
    if(status < 0){
        goto exit;
    }
    // Send frame payload
    status = encode_and_send(size, data);
    if(status < 0){
        goto exit;
    }
    // Send frame checksum
    code = (uint8_t) frame_check_sequence;
    status = encode_and_send(1, &code);
    if(status < 0){
        goto exit;
    }
    code = (uint8_t) (frame_check_sequence >> 8);
    status = encode_and_send(1, &code);
    if(status < 0){
        goto exit;
    }
    // Send frame end code
    code = FRAME_END_CODE;
    status = transport_mac.write(1, &code);

    log_frame(size, data, frame_check_sequence);

    exit:
        return status;
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
