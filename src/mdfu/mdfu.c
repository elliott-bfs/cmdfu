/**
 * @file mdfu.c
 * @author lars.haring@microchip.com
 * @brief MDFU protocol
 * 
 */
#include <stdio.h>
#include <errno.h>
#include <assert.h>
#ifdef __has_include
    #if __has_include(<endian.h>)
        #include <endian.h>
    #else
        #include "mdfu/endian.h"
        #ifdef _WIN32
            #undef ERROR
        #endif
    #endif
#endif
#include <stdlib.h>
#include <string.h>
#include "mdfu/mdfu.h"
#include "mdfu/logging.h"
#include "mdfu/image_reader.h"
#include "mdfu/error.h"

/**
 * @defgroup mdfu_packet MDFU packet definitions
 * @brief Constants related to a MDFU packet
 * @{
 */
/**
 * @def MDFU_HEADER_SYNC
 * @brief Bitmask for the sync bit in the MDFU packet header.
 */
#define MDFU_HEADER_SYNC 0x80
/**
 * @def MDFU_HEADER_RESEND
 * @brief Bitmask for the resend bit in the MDFU packet header.
 */
#define MDFU_HEADER_RESEND 0x40
/**
 * @def MDFU_HEADER_SEQUENCE_NUMBER
 * @brief Bitmask for the sequence number in the MDFU packet header.
 */
#define MDFU_HEADER_SEQUENCE_NUMBER 0x1F
/** @} */ // end of mdfu_packet group


/**
 * @defgroup client_info Client Information
 * @brief Constants related to MDFU client information.
 * @{
 */

/**
 * @def PARAM_TYPE_SIZE
 * @brief Size of the client info parameter type in bytes.
 */
#define PARAM_TYPE_SIZE  1
/**
 * @def PARAM_LENGTH_SIZE
 * @brief Size of the client info parameter length in bytes.
 */
#define PARAM_LENGTH_SIZE  1
/**
 * @def BUFFER_INFO_SIZE
 * @brief Size of the client info buffer information in bytes.
 */
#define BUFFER_INFO_SIZE  3
/**
 * @def PROTOCOL_VERSION_SIZE
 * @brief Size of the protocol version in bytes.
 */
#define PROTOCOL_VERSION_SIZE  3
/**
 * @def PROTOCOL_VERSION_INTERNAL_SIZE
 * @brief Size of the internal protocol version in bytes.
 */
#define PROTOCOL_VERSION_INTERNAL_SIZE  4
/**
 * @def COMMAND_TIMEOUT_SIZE
 * @brief Size of a command timeout in bytes.
 */
#define COMMAND_TIMEOUT_SIZE  3
/**
 * @def INTER_TRANSACTION_DELAY_SIZE
 * @brief Size of the inter transaction delay in bytes.
 */
#define INTER_TRANSACTION_DELAY_SIZE  4
/**
 * @def SECONDS_PER_LSB
 * @brief Number of seconds per least significant bit.
 */
#define SECONDS_PER_LSB  0.1
/**
 * @def LSBS_PER_SECOND
 * @brief Number of least significant bits per second.
 */
#define LSBS_PER_SECOND  10
/**
 * @brief Inter transaction delay default value
 *
 * This delay is used during the initial phase of the session where
 * the host retrieves the client information.
 * This delay is in seconds and the default value is 0.01 seconds.
 */
#define MDFU_INTER_TRANSACTION_DELAY_DEFAULT 0.01
/**
 * @def ITD_SECONDS_PER_LSB
 * @brief Number of seconds per least significant bit for inter transaction delay.
 */
#define ITD_SECONDS_PER_LSB  1e-9
/**
 * @def ITD_LSBS_PER_SECOND
 * @brief Number of least significant bits per second for inter transaction delay.
 */
#define ITD_LSBS_PER_SECOND  1e9
/**
 * @def MDFU_CLIENT_INFO_CMD_TIMEOUT
 * @brief Client info command timeout in seconds.
 */
#define MDFU_CLIENT_INFO_CMD_TIMEOUT 1

/**
 * @enum client_info_type_t
 * @brief Enumeration for different types of client information.
 */
typedef enum {
    PROTOCOL_VERSION = 1,
    BUFFER_INFO = 2,
    COMMAND_TIMEOUT = 3,
    INTER_TRANSACTION_DELAY = 4
}client_info_type_t;

/** @} */ // end of client_info group

/**
 * @enum mdfu_image_state_t
 * @brief Enumeration for the state of the MDFU image.
 */
typedef enum {
    VALID = 1,
    INVALID = 2
}mdfu_image_state_t;

/**
 * @brief MDFU command codes descriptions.
 * 
 */
static const char *MDFU_COMMANDS_STR[] = {
    "", // Command code 0 does not exist
    "Get Client Info",
    "Start Transfer",
    "Write Chunk",
    "Get Image State",
    "End Transfer"
};

/**
 * @brief MDFU status codes descriptions.
 * 
 */
static const char *MDFU_STATUS_STR[] = {
    "", // Status code 0 does not exist
    "Success",
    "Command not supported",
    "", // Reserved for future use
    "Command not executed",
    "Transfer failure",
    "Abort file transfer"
};

/**
 * @brief MDFU file transfer abort cause descriptions.
 * 
 */
const char *MDFU_FILE_TRANSFER_ABORT_CAUSE_STR[] = {
    "Generic problem encountered by client",
    "Generic problem with the update file",
    "The update file is not compatible with the client device ID",
    "An invalid address is present in the update file",
    "Client memory did not properly erase",
    "Client memory did not properly write",
    "Client memory did not properly read",
    "Client did not allow changing to the application version " \
    "in the update file"
};

static const char *MDFU_CMD_NOT_EXECUTED_CAUSE_STR[] = {
    "Command received failed the Transport Integrity Check "\
    "indicating that the command was corrupted during transportation from the host to the client",
    "Received command exceeded the size of the client buffer",
    "Received command was too short",
    "Sequence number of the received command is invalid"
};

static transport_t *mdfu_transport = NULL;
static uint8_t sequence_number;
static int send_retries;
static client_info_t local_client_info;
static bool client_info_valid = false;
static uint8_t cmd_packet_buffer[MDFU_CMD_PACKET_MAX_SIZE];
static uint8_t status_packet_buffer[MDFU_RESPONSE_PACKET_MAX_SIZE];

void mdfu_log_packet(const mdfu_packet_t *packet, mdfu_packet_type_t type);
ssize_t mdfu_encode_cmd_packet(mdfu_packet_t *mdfu_packet);
int mdfu_decode_packet(mdfu_packet_t *mdfu_packet, mdfu_packet_type_t type, int packet_size);
int mdfu_decode_client_info(const uint8_t *data, int length, client_info_t *client_info);

static void log_error_cause(const mdfu_packet_t *status_packet);
int mdfu_send_cmd(mdfu_packet_t *mdfu_cmd_packet, mdfu_packet_t *mdfu_status_packet);

int mdfu_start_transfer(void);
int mdfu_end_transfer(void);
ssize_t mdfu_write_chunk(const image_reader_t* image_reader, int size);
int mdfu_get_image_state(mdfu_image_state_t *state);

/**
 * @brief Increment the MDFU packet sequence number
 * 
 * Will wrap around to 0 according to spec when passing 31.
 */
static inline void increment_sequence_number(){
    sequence_number = (sequence_number + 1) & 0x1F;
}

/**
 * @brief Checks the version against the defined protocol version.
 *
 * This function compares the provided version (major, minor, patch) with the
 * defined protocol version (MDFU_PROTOCOL_VERSION_MAJOR, MDFU_PROTOCOL_VERSION_MINOR,
 * MDFU_PROTOCOL_VERSION_PATCH) and returns:
 * - 1 if the provided version is less than the protocol version,
 * - -1 if the provided version is greater than the protocol version,
 * - 0 if the provided version is equal to the protocol version.
 *
 * @param[in] major Major version number to check.
 * @param[in] minor Minor version number to check.
 * @param[in] patch Patch version number to check.
 * @return 1 if the provided version is less than the protocol version,
 *         -1 if the provided version is greater than the protocol version,
 *         0 if the provided version is equal to the protocol version.
 */
static int version_check(uint8_t major, uint8_t minor, uint8_t patch){
    if (MDFU_PROTOCOL_VERSION_MAJOR < major) return -1;
    if (MDFU_PROTOCOL_VERSION_MAJOR > major) return 1;

    if (MDFU_PROTOCOL_VERSION_MINOR < minor) return -1;
    if (MDFU_PROTOCOL_VERSION_MINOR > minor) return 1;

    if (MDFU_PROTOCOL_VERSION_PATCH < patch) return -1;
    if (MDFU_PROTOCOL_VERSION_PATCH > patch) return 1;

    return 0;
}

/**
 * @brief Sets up buffers for a MDFU transaction.
 *
 * Initializes the buffer pointers in the MDFU packets for a transaction.
 *
 * @param cmd_packet MDFU command packet
 * @param status_packet MDFU status packet
 */
int mdfu_get_packet_buffer(mdfu_packet_t *cmd_packet, mdfu_packet_t *status_packet){
    cmd_packet->buf = cmd_packet_buffer;
    cmd_packet->data = &cmd_packet_buffer[2];
    status_packet->buf = status_packet_buffer;
    status_packet->data = &status_packet_buffer[2];
    return 0;
}

/**
 * @brief Log a MDFU packet
 * 
 * @param packet MDFU packet to log
 * @param type MDFU packet type, either MDFU_CMD or MDFU_STATUS.
 */
void mdfu_log_packet(const mdfu_packet_t *packet, mdfu_packet_type_t type){
    // Best estimate 128 chars for printed text, x2 for data since we print them as hex characters
    char buf[128 + MDFU_MAX_COMMAND_DATA_LENGTH * 2];
    int cnt = 0;
    if(type == MDFU_CMD){
        cnt = sprintf((char *) &buf, "Sequence number: %d; Command: %s; Sync: %s; Data size: %d",
            packet->sequence_number,
            MDFU_COMMANDS_STR[packet->command],
            packet->sync ? "true" : "false",
            packet->data_length);
    } else {
        cnt = sprintf((char *) &buf, "Sequence number: %d; Status: %s; Resend: %s; Data size: %d",
            packet->sequence_number,
            MDFU_STATUS_STR[packet->status],
            packet->resend ? "true" : "false",
            packet->data_length);
    }
    if(packet->data_length) {
        cnt += sprintf(&buf[cnt], ";Data: 0x");
        for(int i = 0; i < packet->data_length; i++){
            cnt += sprintf(&buf[cnt], "%02x", packet->data[i]);
        }
    }
    DEBUG("%s", (char *) &buf);
}

/**
 * @brief Encode a MDFU packet.
 *
 * Encodes the variables from the packet into a bytearray that can be sent
 * via the transport to the client. The encoded packet will be pointed to
 * by mdfu_packet->buf.
 *
 * Note that only the MDFU header is encoded and placed in the buffer while
 * the data must be placed separately.
 *
 * @param mdfu_packet MDFU packet to encode
 * @return Size of encoded MDFU packet in bytes
 */
ssize_t mdfu_encode_cmd_packet(mdfu_packet_t *mdfu_packet){
    assert(32 > mdfu_packet->sequence_number);
    assert(mdfu_packet->command != 0 && mdfu_packet->command < MAX_MDFU_CMD);

    mdfu_packet->buf[0] = mdfu_packet->sequence_number;
    if(mdfu_packet->sync){
        mdfu_packet->buf[0] |= MDFU_HEADER_SYNC;
    }
    mdfu_packet->buf[1] = mdfu_packet->command;
    
    return 2 + mdfu_packet->data_length;
}

/**
 * @brief Decode a MDFU packet
 * 
 * @param mdfu_packet MDFU packet to decode
 * @param type Type of packet, either command (MDFU_CMD) or status (MDFU_STATUS)
 * @param packet_size Size of the packet to decode
 * @return -1 for decoding errors and 0 for success
 */
int mdfu_decode_packet(mdfu_packet_t *mdfu_packet, mdfu_packet_type_t type, int packet_size){
    int status = 0;
    assert(type == MDFU_CMD || type == MDFU_STATUS);

    if(type == MDFU_CMD){
        mdfu_packet->sync = (mdfu_packet->buf[0] & MDFU_HEADER_SYNC) ? true : false;
        mdfu_packet->command = mdfu_packet->buf[1];
        if(mdfu_packet->command == 0 || MAX_MDFU_CMD <= mdfu_packet->command){
            ERROR("Invalid MDFU command %d", mdfu_packet->command);
            status = -1;
        }
    }else{
        mdfu_packet->resend = (mdfu_packet->buf[0] & MDFU_HEADER_RESEND) ? true : false;
        mdfu_packet->status = mdfu_packet->buf[1];
        if(mdfu_packet->status == 0 || MAX_MDFU_STATUS <= mdfu_packet->status){
            ERROR("Invalid MDFU status %d", mdfu_packet->status);
            status = -1;
        }
    }
    mdfu_packet->sequence_number = mdfu_packet->buf[0] & MDFU_HEADER_SEQUENCE_NUMBER;

    if(packet_size > 2){
        mdfu_packet->data = &mdfu_packet->buf[2];
        mdfu_packet->data_length = (uint16_t) packet_size - 2;
    } else {
        mdfu_packet->data = NULL;
        mdfu_packet->data_length = 0;
    }
    return status;
}

/**
 * @brief MDFU initialization
 *
 * @param transport MDFU transport
 * @param retries Number of retries for a MDFU transaction
 * @return int Initialization status. 0 for sucess
 */
int mdfu_init(transport_t *transport, int retries){
    mdfu_transport = transport;
    sequence_number = 0;
    send_retries = retries;
    client_info_valid = false;
    return 0;
}

/**
 * @brief Runs the MDFU firmware update process using the provided image reader.
 *
 * This function performs a series of steps to update the firmware, including
 * retrieving client information, checking protocol version compatibility,
 * setting inter-transaction delays, and writing data chunks. It ensures that
 * the image state is valid before finalizing the transfer.
 *
 * @param image_reader Pointer to the image reader structure that provides the firmware image.
 * @return int Returns 0 on success, or -1 on failure.
 */
int mdfu_run_update(const image_reader_t *image_reader){
    mdfu_image_state_t state;
    ssize_t size;

    if(mdfu_get_client_info(&local_client_info) < 0){
        goto err_exit;
    }
    if(version_check(local_client_info.version.major, local_client_info.version.minor, local_client_info.version.patch) < 0)
    {
        ERROR("MDFU client protocol version %d.%d.%d not supported. "\
            "This MDFU host implements MDFU protocol version %s. "\
            "Please update cmdfu to the latest version.", local_client_info.version.major, local_client_info.version.minor, local_client_info.version.patch, MDFU_PROTOCOL_VERSION);
        goto err_exit;
    }
    if(MDFU_MAX_COMMAND_DATA_LENGTH < local_client_info.buffer_size){
        ERROR("MDFU host protocol buffers are configured for a maximum command data length of %d but the client requires %d", MDFU_MAX_COMMAND_DATA_LENGTH, local_client_info.buffer_size);
        goto err_exit;
    }
    if (mdfu_transport->ioctl != NULL &&
            0 > mdfu_transport->ioctl(TRANSPORT_IOC_INTER_TRANSACTION_DELAY, (float) local_client_info.inter_transaction_delay * ITD_SECONDS_PER_LSB)) {
            goto err_exit;
    }
    client_info_valid = true;
    if(mdfu_start_transfer() < 0){
        goto err_exit;
    }

    do{
        size = mdfu_write_chunk(image_reader, local_client_info.buffer_size);
        if(size < 0){
            goto err_exit;
        }
    // last data chunck read will be zero or less than client buffer size
    }while(size == local_client_info.buffer_size);

    if(mdfu_get_image_state(&state) < 0){
        goto err_exit;
    }
    if(state != VALID){
        ERROR("Image state %d is invalid", state);
        goto err_exit;
    }
    if(mdfu_end_transfer() < 0){
        goto err_exit;
    }
    return 0;

    err_exit:
    return -1;
}

/**
 * @brief Starts a data transfer.
 *
 * This function sends a START_TRANSFER command to initiate a data transfer.
 *
 * @return 0 on success, -1 on failure.
 */
int mdfu_start_transfer(void){
    mdfu_packet_t mdfu_status_packet;
    mdfu_packet_t mdfu_cmd_packet = {
        .command = START_TRANSFER,
        .sync = false,
        .data_length = 0
    };
    mdfu_get_packet_buffer(&mdfu_cmd_packet, &mdfu_status_packet);

    if(mdfu_send_cmd(&mdfu_cmd_packet, &mdfu_status_packet) < 0){
        return -1;
    }
    return 0;
}

/**
 * @brief Ends a data transfer.
 *
 * This function sends an END_TRANSFER command to terminate a data transfer.
 *
 * @return 0 on success, -1 on failure.
 */
int mdfu_end_transfer(void){
    mdfu_packet_t mdfu_status_packet;
    mdfu_packet_t mdfu_cmd_packet = {
        .command = END_TRANSFER,
        .sync = false,
        .data_length = 0
    };
    mdfu_get_packet_buffer(&mdfu_cmd_packet, &mdfu_status_packet);
    if(mdfu_send_cmd(&mdfu_cmd_packet, &mdfu_status_packet) < 0){
        return -1;
    }
    return 0;
}

/**
 * @brief Retrieves the current state of the image.
 *
 * This function sends a GET_IMAGE_STATE command and updates the provided state variable
 * with the current image state.
 *
 * @param[out] state Pointer to a variable to store the current image state.
 * @return 0 on success, -1 on failure.
 */
int mdfu_get_image_state(mdfu_image_state_t *state){
    mdfu_packet_t mdfu_status_packet;
    mdfu_packet_t mdfu_cmd_packet = {
        .command = GET_IMAGE_STATE,
        .sync = false,
        .data_length = 0
    };
    mdfu_get_packet_buffer(&mdfu_cmd_packet, &mdfu_status_packet);
    if(mdfu_send_cmd(&mdfu_cmd_packet, &mdfu_status_packet) < 0){
        return -1;
    }
    assert(mdfu_status_packet.data != NULL);
    *state = mdfu_status_packet.data[0];
    return 0;
}

/**
 * @brief Writes a chunk of firmware update image data.
 *
 * This function reads a chunk of data from the provided image reader and sends a WRITE_CHUNK
 * command with the read data. Call this function repeatedly to send image data chunks
 * until the whole image is transferred. For the last data chunk the function will return
 * less than the requested size.
 *
 * @param[in] image_reader Pointer to an image reader structure.
 * @param[in] size The size of the data chunk to read and write.
 * @return The number of bytes read and written on success, -1 on failure.
 */
ssize_t mdfu_write_chunk(const image_reader_t *image_reader, int size){
    mdfu_packet_t mdfu_cmd_packet = {
        .command = WRITE_CHUNK,
        .sync = false,
    };
    mdfu_packet_t mdfu_status_packet;
    mdfu_get_packet_buffer(&mdfu_cmd_packet, &mdfu_status_packet);
    ssize_t read_size;

    read_size = image_reader->read(mdfu_cmd_packet.data, size);
    if(0 > read_size){
        ERROR("%s", strerror(errno));
        return -1;
    }
    if(0 != read_size){
        mdfu_cmd_packet.data_length = (uint16_t) read_size;
        if(mdfu_send_cmd(&mdfu_cmd_packet, &mdfu_status_packet) < 0){
            return -1;
        }
    }
    return read_size;
}

/**
 * @brief Sends a command packet and waits for a status packet response.
 *
 * This function encodes and sends a command packet, then waits for a status packet response.
 * It handles retries and timeouts based on the client information and command type.
 *
 * @param[in] mdfu_cmd_packet Pointer to the command packet to be sent.
 * @param[out] mdfu_status_packet Pointer to the status packet to be received.
 * @return int 0 on success, negative error code on failure.
 */
int mdfu_send_cmd(mdfu_packet_t *mdfu_cmd_packet, mdfu_packet_t *mdfu_status_packet){
    int status;
    int cmd_packet_size;
    int status_packet_size;
    int retries = send_retries;
    float cmd_timeout = MDFU_CLIENT_INFO_CMD_TIMEOUT;

    if(client_info_valid){
        cmd_timeout = (float) (local_client_info.cmd_timeouts[mdfu_cmd_packet->command - 1] * SECONDS_PER_LSB);
    }
    if(mdfu_cmd_packet->sync){
        sequence_number = 0;
    }
    mdfu_cmd_packet->sequence_number = sequence_number;

    cmd_packet_size = (int) mdfu_encode_cmd_packet(mdfu_cmd_packet);

    DEBUG("Sending MDFU command packet");
    mdfu_log_packet(mdfu_cmd_packet, MDFU_CMD);

    while(retries){
        retries -= 1;
        status = mdfu_transport->write(cmd_packet_size, mdfu_cmd_packet->buf);
        if(status < 0){
            continue;
        }
        status = mdfu_transport->read(&status_packet_size, mdfu_status_packet->buf, cmd_timeout);
        if(status < 0){
            continue;
        }
        mdfu_decode_packet(mdfu_status_packet, MDFU_STATUS, status_packet_size);
        DEBUG("Received MDFU status packet");
        mdfu_log_packet(mdfu_status_packet, MDFU_STATUS);

        if(mdfu_status_packet->resend){
            DEBUG("Client requested resending MDFU packet with sequence number %d", mdfu_status_packet->sequence_number);
            continue;
        }

        increment_sequence_number();

        if(mdfu_status_packet->status != SUCCESS){
            log_error_cause(mdfu_status_packet);
            status = -EPROTO;
        }
        break;
    }
    if(retries == 0){
        ERROR("Tried %d times to send command without success", send_retries);
        status = -EIO;
    }
    return status;
}

/**
 * @brief Log detailed error for MDFU status packet.
 * 
 * @param status_packet MDFU status packet returned from client.
 */
static void log_error_cause(const mdfu_packet_t *status_packet){
    assert(status_packet->data != NULL);
    ERROR("Received MDFU status packet with %s", MDFU_STATUS_STR[status_packet->status]);

    if(COMMAND_NOT_EXECUTED == status_packet->status){
        if(MAX_CMD_NOT_EXECUTED_ERROR_CAUSE <= status_packet->data[0]){
            ERROR("Invalid command not executed cause %d", status_packet->data[0]);
        }else{
            ERROR("Command not executed cause: %s", MDFU_CMD_NOT_EXECUTED_CAUSE_STR[status_packet->data[0]]);
        }
    }else if(ABORT_FILE_TRANSFER == status_packet->status){
        if(MAX_FILE_TRANSFER_ABORT_CAUSE <= status_packet->data[0]){
            ERROR("Invalid file transfer abort cause %d", status_packet->data[0]);
        }else{
            ERROR("File transfer abort cause: %s", MDFU_FILE_TRANSFER_ABORT_CAUSE_STR[status_packet->data[0]]);
        }
    }
}

/**
 * @brief Decodes the MDFU client protocol version from the provided data.
 *
 * This function decodes the major, minor, and patch version numbers from the given data.
 * If the length is 4, it also decodes the internal version number.
 *
 * @param[in,out] client_info Pointer to the client_info_t structure where the decoded information will be stored.
 * @param[in] length Length of the data buffer. Must be 3 or 4.
 * @param[in] data Pointer to the data buffer containing the encoded protocol version.
 * @return 0 on success, -1 on failure (e.g., if the length is not 3 or 4).
 */
int mdfu_decode_protcol_version(client_info_t *client_info, const uint8_t length, const uint8_t *data){
    if(3 == length || 4 == length){
        client_info->version.major = data[0];
        client_info->version.minor = data[1];
        client_info->version.patch = data[2];
        if(4 == length){
            client_info->version.internal = data[3];
            client_info->version.internal_present = true;
        }else{
            client_info->version.internal_present = false;
        }
    }else {
        ERROR("Invalid parameter length for client protocol version. Expected 3 or 4 but got %d", length);
        return -1;
    }
    return 0;
}

/**
 * @brief Decodes the MDFU client command timeouts from the provided data.
 *
 * This function decodes the command-specific timeouts from the given data and stores them
 * in the provided client_info structure. The first timeout in the data must be the default timeout.
 *
 * @param[in,out] client_info Pointer to the client_info_t structure where the decoded information will be stored.
 * @param[in] length Length of the data buffer. Must be a multiple of COMMAND_TIMEOUT_SIZE.
 * @param[in] data Pointer to the data buffer containing the encoded command timeouts.
 * @return 0 on success, -1 on failure (e.g., if the length is not a multiple of COMMAND_TIMEOUT_SIZE).
 */
int mdfu_decode_command_timeout(client_info_t *client_info, const uint8_t length, const uint8_t *data){
    if(length % COMMAND_TIMEOUT_SIZE){
        ERROR("Invalid parameter length for MDFU client command timeouts. Expected length to be a multiple of 3 but got %d", length);
        return -1;
    }
    for(int timeouts = 0; timeouts < (length / 3); timeouts++)
    {
        mdfu_command_t cmd = data[timeouts * COMMAND_TIMEOUT_SIZE];
        uint16_t timeout = le16toh(*(const uint16_t *) &data[1 + timeouts * COMMAND_TIMEOUT_SIZE]);

        if(0 == cmd){ //default timeout
            // Ensure that default timeout is the first timeout that we get
            if(0 != timeouts){
                ERROR("Default client command timeout must be first in the parameter list but it is at position %d", timeouts);
                return -1;
            }
            client_info->default_timeout = timeout;
            // initialize all timeouts to default timeout
            for(int x = 0; x < MAX_MDFU_CMD; x++){
                client_info->cmd_timeouts[x] = timeout;
            }
        }else if(MAX_MDFU_CMD <= cmd){
            ERROR("Invalid command code 0x%x in MDFU client command timeouts", cmd);
            return -1;
        }else{
            client_info->cmd_timeouts[cmd] = timeout;
        }
    }
    return 0;
}

/**
 * @brief Decodes the MDFU client buffer information from the provided data.
 *
 * This function decodes the buffer size and buffer count from the given data
 * and stores them in the provided client_info structure.
 *
 * @param[in,out] client_info Pointer to the client_info_t structure where the decoded information will be stored.
 * @param[in] length Length of the data buffer. Must be equal to BUFFER_INFO_SIZE.
 * @param[in] data Pointer to the data buffer containing the encoded buffer information.
 * @return 0 on success, -1 on failure (e.g., if the length is not equal to BUFFER_INFO_SIZE).
 */
int mdfu_decode_buffer_info(client_info_t *client_info, const uint8_t length, const uint8_t *data){
    if(length != BUFFER_INFO_SIZE){
        ERROR("Invalid parameter length for MDFU client buffer info. Expected %d but got %d", BUFFER_INFO_SIZE, length);
        return -1;
    }
    client_info->buffer_size = le16toh(*((const uint16_t *) data));
    client_info->buffer_count = data[2];
    return 0;
}

/**
 * @brief Decodes the MDFU client inter-transaction delay from the provided data.
 *
 * This function decodes the inter-transaction delay from the given data
 * and stores it in the provided client_info structure.
 *
 * @param[in,out] client_info Pointer to the client_info_t structure where the decoded information will be stored.
 * @param[in] length Length of the data buffer. Must be equal to INTER_TRANSACTION_DELAY_SIZE.
 * @param[in] data Pointer to the data buffer containing the encoded inter-transaction delay.
 * @return 0 on success, -1 on failure (e.g., if the length is not equal to INTER_TRANSACTION_DELAY_SIZE).
 */
int mdfu_decode_inter_transaction_delay(client_info_t *client_info, const uint8_t length, const uint8_t *data){
    if(length != INTER_TRANSACTION_DELAY_SIZE){
        ERROR("Invalid parameter length for MDFU inter transaction delay. Expected %d bug got %d", INTER_TRANSACTION_DELAY_SIZE, length);
        return -1;
    }
    client_info->inter_transaction_delay = le32toh(*((const uint32_t *) data));
    return 0;
}

/**
 * @brief Decodes client info data
 * 
 * @param data Encoded client info data from client.
 * @param length Length of the encoded data.
 * @param client_info Decoded client info
 * @return int Success=0, Error=-1
 */
int mdfu_decode_client_info(const uint8_t *data, int length, client_info_t *client_info)
{
    int status = 0;
    uint8_t parameter_length;
    const uint8_t *parameter_data;
    client_info_type_t parameter_type;

    for(int i = 0; i < length;)
    {
        parameter_type = data[i];
        parameter_length = data[i + 1];
        parameter_data = &data[i + 2];
        i = i + 2 + parameter_length;

        if(i > length){
            ERROR("MDFU client info parameter length exceeds available data");
            return -1;
        }

        switch(parameter_type)
        {
            case PROTOCOL_VERSION:
                status = mdfu_decode_protcol_version(client_info, parameter_length, parameter_data);
                break;

            case BUFFER_INFO:
                status = mdfu_decode_buffer_info(client_info, parameter_length, parameter_data);
                break;

            case COMMAND_TIMEOUT:
                status = mdfu_decode_command_timeout(client_info, parameter_length, parameter_data);
                break;

            case INTER_TRANSACTION_DELAY:
                status = mdfu_decode_inter_transaction_delay(client_info, parameter_length, parameter_data);
                break;

            default:
                ERROR("Invalid MDFU client info parameter type %d", parameter_type);
                status = -1;
        }
        if(status < 0){
            return -1;
        }
    }
    return 0;
}

/**
 * @brief Get MDFU client info
 * 
 * @param client_info Pointer to client_info_t struct to store the data.
 * @return int Success=0, Error=-1
 */
int mdfu_get_client_info(client_info_t *client_info){
    mdfu_packet_t mdfu_status_packet;
    mdfu_packet_t mdfu_cmd_packet = {
        .command = GET_CLIENT_INFO,
        .sync = true,
        .sequence_number = 0,
        .data_length = 0
    };
    mdfu_get_packet_buffer(&mdfu_cmd_packet, &mdfu_status_packet);
    // Configure default transport layer inter transaction delay for transports
    // that support it
    if(mdfu_transport->ioctl != NULL &&
        0 > mdfu_transport->ioctl(TRANSPORT_IOC_INTER_TRANSACTION_DELAY, MDFU_INTER_TRANSACTION_DELAY_DEFAULT)){
        return -1;
    }
    if(mdfu_send_cmd(&mdfu_cmd_packet, &mdfu_status_packet) < 0){
        return -1;
    }
    if(mdfu_decode_client_info(mdfu_status_packet.data, mdfu_status_packet.data_length, client_info) < 0){
        return -1;
    }
    return 0;
}

/**
 * @brief Print client info in human readable form to stdout
 * 
 * @param client_info Pointer to client_info_t struct
 */
void print_client_info(const client_info_t *client_info){
    char internal_version[6];
    if(client_info->version.internal_present)
        sprintf((char *) &internal_version, "-%d", client_info->version.internal);
    else{
        internal_version[0] = '\0';
    }
    printf(\
    "MDFU client information\n"
    "--------------------------------\n"
    "- MDFU protocol version: %d.%d.%d%s\n"
    "- Number of command buffers: %d\n"
    "- Maximum packet data length: %d bytes\n"
    "- Inter transaction delay: %f seconds\n"
    "Command timeouts\n"
    "- Default timeout: %.1f seconds\n",
    client_info->version.major,
    client_info->version.minor,
    client_info->version.patch,
    internal_version,
    client_info->buffer_count,
    client_info->buffer_size,
    client_info->inter_transaction_delay * ITD_SECONDS_PER_LSB,
    client_info->default_timeout * SECONDS_PER_LSB);
    for(int i = 1; i < MAX_MDFU_CMD; i++){
        printf("- %s: %.1f seconds\n", MDFU_COMMANDS_STR[i], client_info->cmd_timeouts[i-1] * SECONDS_PER_LSB);
    }
}

/**
 * @brief Open MDFU protocol layer.
 * 
 * This operation must be done before accessing any other MDFU
 * API operation.
 * 
 * @return int, 0 for success and -1 for error.
 */
int mdfu_open(void){
    int status = 0;

    if(NULL != mdfu_transport){
        if(mdfu_transport->open() < 0){
            DEBUG("MDFU failed to open transport");
            status = -1;
        }
    }else{
        status = -1;
    }

    return status;
}

/**
 * @brief Close MDFU protocol layer.
 * 
 * @return int, 0 for success and -1 for error.
 */
int mdfu_close(void){
    int status = 0;

    if(NULL != mdfu_transport){
        if(0 > mdfu_transport->close()){
            DEBUG("MDFU failed to close transport");
            status = -1;
        }
    }else{
        status = -1;
    }
    return status;
}
