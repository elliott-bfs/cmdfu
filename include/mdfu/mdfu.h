#ifndef MDFU_H
#define MDFU_H
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "mdfu/transport.h"

#define MDFU_COMMAND_SIZE 1
#define MDFU_SEQUENCE_FIELD_SIZE 1
#define MDFU_DATA_MAX_SIZE 1024
#define MDFU_PACKET_BUFFER_SIZE (MDFU_SEQUENCE_FIELD_SIZE + MDFU_COMMAND_SIZE + MDFU_DATA_MAX_SIZE)

#define MDFU_HEADER_SYNC 0x80
#define MDFU_HEADER_RESEND 0x40
#define MDFU_HEADER_SEQUENCE_NUMBER 0x1F

typedef enum
{
    GET_CLIENT_INFO = 0x01U,
    START_TRANSFER = 0x02U,
    WRITE_CHUNK = 0x03U,
    GET_IMAGE_STATE = 0x04U,
    END_TRANSFER = 0x05U,
    MAX_MDFU_CMD = 0x06U // Indicates max enum value
} mdfu_command_t;

typedef enum
{
    SUCCESS = 0x01U,
    NOT_SUPPORTED = 0x02U,
    NOT_AUTHORIZED = 0x03U,
    PACKET_TRANSPORT_FAILURE = 0x04U,
    ABORT_FILE_TRANSFER = 0x05U,
    MAX_MDFU_STATUS = 0x06U // Indicates max enum value
} mdfu_status_t;

typedef enum{
    GENERIC_CLIENT_ERROR = 0x00U,
    INVALID_FILE = 0x01U,
    INVALID_CLIENT_DEVICE_ID = 0x02U,
    ADDRESS_ERROR = 0x04U,
    ERASE_ERROR = 0x04U,
    WRITE_ERROR = 0x05U,
    READ_ERROR = 0x06U,
    APPLICATION_VERSION_ERROR = 0x07U,
    MAX_FILE_TRANSFER_ABORT_CAUSE = 0x08U // Indicates max enum value
}mdfu_file_transfer_abort_cause_t;

typedef enum {
    INVALID_CHECKSUM = 0,
    PACKET_TOO_LARGE = 1,
    MAX_TRANSPORT_ERROR_CAUSE = 2
}transport_error_cause_t;

typedef struct
{
    uint8_t sequence_number;
    bool sync;
    bool resend;
    union
    {
        uint8_t command;
        uint8_t status;
    };
    uint16_t data_length;
    uint8_t *data;
}mdfu_packet_t;

typedef struct packet_buffer {
    int size;
    uint8_t buffer[MDFU_PACKET_BUFFER_SIZE];
}mdfu_packet_buffer_t;

typedef enum {
    MDFU_CMD = 0,
    MDFU_STATUS = 1
} mdfu_packet_type_t;

typedef struct{
    struct {
        uint8_t major;
        uint8_t minor;
        uint8_t patch;
        uint8_t internal;
        bool internal_present;
    }version;
    uint8_t buffer_count;
    uint16_t buffer_size;
    uint16_t default_timeout;
    uint16_t cmd_timeouts[MAX_MDFU_CMD - 1];
}client_info_t;

void mdfu_encode_cmd_packet(mdfu_packet_t *mdfu_packet, mdfu_packet_buffer_t *mdfu_packet_buffer);
void mdfu_log_packet(mdfu_packet_t *packet, mdfu_packet_type_t type);
void mdfu_decode_packet(mdfu_packet_t *packet, mdfu_packet_type_t type, mdfu_packet_buffer_t *packet_buffer);
void mdfu_encode_cmd_packet_cp(mdfu_packet_t *mdfu_packet, uint8_t *encoded_packet, int *encoded_packet_size);
int mdfu_decode_packet_cp(mdfu_packet_t *decoded_packet, mdfu_packet_type_t type, uint8_t *packet, int packet_size);
int mdfu_decode_client_info(uint8_t *data, int length, client_info_t *client_info);
void print_client_info(client_info_t *client_info);
int mdfu_init(transport_t *transport, int retries);
int mdfu_open(void);
int mdfu_close(void);
int mdfu_get_client_info(client_info_t *client_info);
int mdfu_run_update(FILE *image);
#endif