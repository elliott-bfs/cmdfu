/**
 * @file mdfu.c
 * @author lars.haring@microchip.com
 * @brief MDFU protocol
 * 
 */
#include <stdio.h>
#include <errno.h>
#include <assert.h>
#include <endian.h>
#include "mdfu/mdfu.h"
#include "mdfu/logging.h"

// Client info 
#define PARAM_TYPE_SIZE  1
#define PARAM_LENGTH_SIZE  1
#define BUFFER_INFO_SIZE  3
#define PROTOCOL_VERSION_SIZE  3
#define PROTOCOL_VERSION_INTERNAL_SIZE  4
#define COMMAND_TIMEOUT_SIZE  3
#define SECONDS_PER_LSB  0.1
#define LSBS_PER_SECOND  10

typedef enum {
    PROTOCOL_VERSION = 1,
    BUFFER_INFO = 2,
    COMMAND_TIMEOUT = 3
}client_info_type_t;

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
    "Command Not Supported",
    "Command Not Authorized",
    "Transfer Failure",
    "Abort File Transfer"
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

static const char *MDFU_TRANSPORT_ERROR_CAUSE_STR[] = {
    "Invalid checksum detected",
    "Packet was too large"
};

static uint16_t chunk_size;
static transport_t *mdfu_transport;
static uint8_t sequence_number;
static int send_retries;
static client_info_t client_info;
static bool client_info_valid = false;
static float default_timeout = 1;

static void log_error_cause(mdfu_packet_t *status_packet);
int mdfu_send_cmd(mdfu_packet_t *mdfu_cmd_packet, mdfu_packet_t *mdfu_status_packet);

/**
 * @brief Increment the MDFU packet sequence number
 * 
 * Will wrap around to 0 according to spec when passing 31.
 */
static inline void increment_sequence_number(){
    sequence_number = (sequence_number + 1) & 0x1F;
}

/**
 * @brief Log a MDFU packet
 * 
 * @param packet MDFU packet to log
 * @param type MDFU packet type, either MDFU_CMD or MDFU_STATUS.
 */
void mdfu_log_packet(mdfu_packet_t *packet, mdfu_packet_type_t type){
    char buf[128 + MDFU_DATA_MAX_SIZE * 2];
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
        cnt += sprintf((char *) &buf[cnt], ";Data: 0x");
        for(int i = 0; i < packet->data_length; i++){
            cnt += sprintf((char *) &buf[cnt], "%02x", packet->data[i]);
        }
    }
    DEBUG("%s", (char *) &buf);
}

void mdfu_encode_cmd_packet(mdfu_packet_t *mdfu_packet, mdfu_packet_buffer_t *mdfu_packet_buffer){
    // TODO verify parameters
    // sequence number between 0 and 31
    // command in known commands
    // status in known status
    if(mdfu_packet->sync){
        mdfu_packet_buffer->buffer[0] |= MDFU_HEADER_SYNC;
    }
    mdfu_packet_buffer->buffer[1] = mdfu_packet->command;
    
    for(int i=0; i < mdfu_packet->data_length; i++)
    {
        mdfu_packet_buffer->buffer[i+2] = mdfu_packet->data[i];
    }
}

/**
 * @brief Encode a MDFU packet.
 * 
 * @param mdfu_packet MDFU packet to encode
 * @param encoded_packet Encoded MDFU packet
 * @param encoded_packet_size Size of the encoded MDFU packet
 */
void mdfu_encode_cmd_packet_cp(mdfu_packet_t *mdfu_packet, uint8_t *encoded_packet, int *encoded_packet_size){
    uint8_t *buffer = (uint8_t *) encoded_packet;

    assert(32 > mdfu_packet->sequence_number);
    assert(mdfu_packet->command != 0 && mdfu_packet->command < MAX_MDFU_CMD);

    buffer[0] = mdfu_packet->sequence_number;
    if(mdfu_packet->sync){
        buffer[0] |= MDFU_HEADER_SYNC;
    }
    buffer[1] = mdfu_packet->command;
    
    for(int i=0; i < mdfu_packet->data_length; i++)
    {
        buffer[i+2] = mdfu_packet->data[i];
    }
    *encoded_packet_size = 2 + mdfu_packet->data_length;
}

/**
 * @brief Decode a MDFU packet
 * 
 * The packet data payload will be copied to the buffer pointed to by decoded_packet.
 * 
 * @param decoded_packet Decoded MDFU packet
 * @param type Type of packet, either command (MDFU_CMD) or status (MDFU_STATUS)
 * @param packet Raw packet to decode
 * @param packet_size Size of the packet to decode
 */
int mdfu_decode_packet_cp(mdfu_packet_t *decoded_packet, mdfu_packet_type_t type, uint8_t *packet, int packet_size){
    assert(type == MDFU_CMD || type == MDFU_STATUS);

    if(type == MDFU_CMD){
        decoded_packet->sync = (packet[0] & MDFU_HEADER_SYNC) ? true : false;
        decoded_packet->command = packet[1];
        if(decoded_packet->command == 0 || MAX_MDFU_CMD <= decoded_packet->command){
            ERROR("Invalid MDFU command %d", decoded_packet->command);
            return -1;
        }
    }else{
        decoded_packet->resend = (packet[0] & MDFU_HEADER_RESEND) ? true : false;
        decoded_packet->status = packet[1];
        if(decoded_packet->status == 0 || MAX_MDFU_STATUS <= decoded_packet->status){
            ERROR("Invalid MDFU status %d", decoded_packet->status);
            return -1;
        }
    }
    decoded_packet->sequence_number = packet[0] & MDFU_HEADER_SEQUENCE_NUMBER;

    if(packet_size > 2){
        decoded_packet->data = &packet[2];
        decoded_packet->data_length = packet_size - 2;
    } else {
        decoded_packet->data = NULL;
        decoded_packet->data_length = 0;
    }
    return 0;
}

void mdfu_decode_packet(mdfu_packet_t *packet, mdfu_packet_type_t type, mdfu_packet_buffer_t *packet_buffer){
    // TODO verify parameters
    // sequence number between 0 and 31
    // command in known commands
    // status in known status
    if(type == MDFU_CMD){
        packet->sync = (packet_buffer->buffer[0] & MDFU_HEADER_SYNC) ? true : false;
        packet->command = packet_buffer->buffer[1];
    }else{
        packet->resend = (packet_buffer->buffer[0] & MDFU_HEADER_RESEND) ? true : false;
        packet->status = packet_buffer->buffer[1];
    }
    packet->sequence_number = packet_buffer->buffer[0] & MDFU_HEADER_SEQUENCE_NUMBER;
    if(packet_buffer->size > 2){
        packet->data = &packet_buffer->buffer[2];
        packet->data_length = packet_buffer->size - 2;
    } else {
        packet->data = NULL;
        packet->data_length = 0;
    }
}

int mdfu_init(transport_t *transport, int retries){
    mdfu_transport = transport;
    sequence_number = 0;
    send_retries = retries;
    client_info_valid = false;
}

// TODO it would be better to send in a struct containing functions for reading chunks
// this would be a good abstraction because then we can offer multiple ways to
// read the image file piecewise instead of everything at once
int mdfu_run_update(FILE *image){
    mdfu_packet_t mdfu_cmd_packet;
    mdfu_packet_t mdfu_response_packet;

    if(mdfu_transport->open() < 0){
        perror("MDFU update failed: ");
    }
    if(mdfu_get_client_info(&client_info) < 0){
        return -1;
    }
    client_info_valid = true;
    
    mdfu_transport->close();
}
int mdfu_start_transfer(void){
    mdfu_packet_t mdfu_status_packet;
    mdfu_packet_t mdfu_cmd_packet = {
        .command = START_TRANSFER,
        .sync = true,
        .data_length = 0
    };

    if(mdfu_send_cmd(&mdfu_cmd_packet, &mdfu_status_packet) < 0){
        return -1;
    }
}
/**
 * @brief 
 * 
 * @param mdfu_cmd_packet 
 * @param mdfu_status_packet 
 * @return int 
 */
int mdfu_send_cmd(mdfu_packet_t *mdfu_cmd_packet, mdfu_packet_t *mdfu_status_packet){
    mdfu_packet_buffer_t packet_buffer;
    mdfu_packet_buffer_t rx_packet_buffer;
    int status;
    int retries = send_retries;
    float cmd_timeout = default_timeout;

    if(client_info_valid){
        cmd_timeout = client_info.cmd_timeouts[mdfu_cmd_packet->command] * SECONDS_PER_LSB;
    }
    mdfu_cmd_packet->sequence_number = mdfu_cmd_packet->sync ? sequence_number : 0;

    mdfu_encode_cmd_packet_cp(mdfu_cmd_packet, (uint8_t *) &packet_buffer.buffer, &packet_buffer.size);

    DEBUG("Sending MDFU command packet\n");
    mdfu_log_packet(mdfu_cmd_packet, MDFU_CMD);

    while(retries){
        retries -= 1;
        status = mdfu_transport->write(packet_buffer.size, packet_buffer.buffer);
        if(status < 0){
            break;
        }
        status = mdfu_transport->read(&rx_packet_buffer.size, rx_packet_buffer.buffer, cmd_timeout);
        if(status < 0){
            break;
        }
        mdfu_decode_packet_cp(mdfu_status_packet, MDFU_STATUS, (uint8_t *) &rx_packet_buffer.buffer, rx_packet_buffer.size);
        DEBUG("Received MDFU status packet\n");
        mdfu_log_packet(mdfu_status_packet, MDFU_STATUS);

        if(mdfu_status_packet->resend){
            DEBUG("Client requested resending MDFU packet with sequence number %d", mdfu_status_packet->sequence_number);
            continue;
        } else if(mdfu_status_packet->status == SUCCESS) {
            increment_sequence_number();
            break;
        } else {
            increment_sequence_number();
            log_error_cause(mdfu_status_packet);
            status = -EPROTO;
            break;
        }
    }
    if(retries == 0){
        ERROR("Tried %d times to send command without success", retries);
        status = -EIO;
    }
    return status;
}
/**
 * @brief Log detailed error for MDFU status packet.
 * 
 * @param status_packet MDFU status packet returned from client.
 */
static void log_error_cause(mdfu_packet_t *status_packet){
    ERROR("Received MDFU status packet with %s", MDFU_STATUS_STR[status_packet->status]);

    if(PACKET_TRANSPORT_FAILURE == status_packet->status){
        if(MAX_TRANSPORT_ERROR_CAUSE >= status_packet->data[0]){
            ERROR("Unknown transport failure cause %d", status_packet->data[0]);
        }else{
            ERROR("Transport error cause: %s", MDFU_TRANSPORT_ERROR_CAUSE_STR[status_packet->data[0]]);
        }
    }else if(ABORT_FILE_TRANSFER == status_packet->status){
        if(MAX_FILE_TRANSFER_ABORT_CAUSE >= status_packet->data[0]){
            ERROR("Unknown file abort cause %d", status_packet->data[0]);
        }else{
            ERROR("File transfer abort cause: %s", MDFU_FILE_TRANSFER_ABORT_CAUSE_STR[status_packet->data[0]]);
        }
    }
}

/**
 * @brief Decodes client info data
 * 
 * @param data Encoded client info data from client.
 * @param length Length of the encoded data.
 * @param client_info Decoded client info
 * @return int Success=0, Error=-1
 */
int mdfu_decode_client_info(uint8_t *data, int length, client_info_t *client_info)
{
    uint8_t parameter_length;
    client_info_type_t parameter_type;

    for(int i = 0; i < length;)
    {
        parameter_type = data[i];
        parameter_length = data[i + 1];
        i += 2;

        if((i + parameter_length) > length){
            ERROR("MDFU client info parameter length exceeds available data");
            return -1;
        }

        switch(parameter_type)
        {
            case PROTOCOL_VERSION:
                if(3 == parameter_length || 4 == parameter_length){
                    client_info->version.major = data[i];
                    client_info->version.minor = data[i + 1];
                    client_info->version.patch = data[i + 2];
                    if(4 == parameter_length){
                        client_info->version.internal = data[i + 3];
                        client_info->version.internal_present = true;
                    }else{
                        client_info->version.internal_present = false;
                    }
                }else {
                    ERROR("Invalid parameter length for client protocol version. Expected 3 or 4 but got %d", parameter_length);
                    return -1;
                }
                break;

            case BUFFER_INFO:
                if(parameter_length != BUFFER_INFO_SIZE){
                    ERROR("Invalid parameter length for MDFU client buffer info. Expected %d but got %d", BUFFER_INFO_SIZE, parameter_length);
                    return -1;
                }
                client_info->buffer_size = le16toh(*((uint16_t *) &data[i]));
                client_info->buffer_count = data[i + 2];
                break;

            case COMMAND_TIMEOUT:
                if(parameter_length % COMMAND_TIMEOUT_SIZE){
                    ERROR("Invalid parameter length for MDFU client command timeouts. Expected length to be a multiple of 3 but got %d", parameter_length);
                    return -1;
                }
                for(int timeouts = 0; timeouts < (parameter_length / 3); timeouts++)
                {
                    mdfu_command_t cmd = data[i + timeouts * COMMAND_TIMEOUT_SIZE];
                    uint16_t timeout = le16toh(*(uint16_t *) &data[1 + i + timeouts * COMMAND_TIMEOUT_SIZE]);

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
                break;
            default:
                ERROR("Invalid MDFU client info parameter type %d", parameter_type);
                return -1;
        }
        i += parameter_length;
    }
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
void print_client_info(client_info_t *client_info){
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
    "- Number of command buffers: %d bytes\n"
    "- Maximum packet data length: %d\n"
    "Command timeouts\n"
    "- Default timeout: %.1f seconds\n",
    client_info->version.major,
    client_info->version.minor,
    client_info->version.patch,
    internal_version,
    client_info->buffer_count,
    client_info->buffer_size,
    client_info->default_timeout * SECONDS_PER_LSB);
    for(int i = 1; i < MAX_MDFU_CMD; i++){
        printf("- %s: %.1f seconds\n", MDFU_COMMANDS_STR[i], client_info->cmd_timeouts[i] * SECONDS_PER_LSB);
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
    if(mdfu_transport->open() < 0){
        DEBUG("MDFU failed to open transport");
        return -1;
    }
    return 0;
}

/**
 * @brief Close MDFU protocol layer.
 * 
 * @return int, 0 for success and -1 for error.
 */
int mdfu_close(void){
    if(0 > mdfu_transport->close()){
        DEBUG("MDFU failed to close transport");
        return -1;
    }
    return 0;
}

void get_next_chunk(uint8_t *buffer, int size){

}