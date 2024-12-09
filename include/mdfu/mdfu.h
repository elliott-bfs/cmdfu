#ifndef MDFU_H
#define MDFU_H
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "mdfu/transport/transport.h"
#include "mdfu/image_reader.h"
#include "mdfu/mdfu_config.h"

/**
 * @def MDFU_COMMAND_SIZE
 * @brief Size of the MDFU command in the packet header.
 */
#define MDFU_COMMAND_SIZE 1
/**
 * @def MDFU_SEQUENCE_FIELD_SIZE
 * @brief Size in bytes of the sequence field in the packet header.
 */
#define MDFU_SEQUENCE_FIELD_SIZE 1
/**
 * @def MDFU_RESPONSE_STATUS_CODES_SIZE
 * @brief Size in bytes for the repsonse status code field.
 */
#define MDFU_RESPONSE_STATUS_CODES_SIZE 1
/**
 * @def MDFU_CMD_PACKET_MAX_SIZE
 * @brief Size in bytes of the largest expected MDFU command packet.
 * 
 * This size defines the maximum supported MDFU command packet.
 */
#define MDFU_CMD_PACKET_MAX_SIZE (MDFU_SEQUENCE_FIELD_SIZE + MDFU_COMMAND_SIZE + MDFU_MAX_COMMAND_DATA_LENGTH)

/**
 * @def MDFU_RESPONSE_PACKET_MAX_SIZE
 * @brief Sinze in bytes for the maximum MDFU repsonse
 */
#define MDFU_RESPONSE_PACKET_MAX_SIZE (MDFU_SEQUENCE_FIELD_SIZE + MDFU_RESPONSE_STATUS_CODES_SIZE + MDFU_MAX_RESPONSE_DATA_LENGTH)

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
    COMMAND_NOT_SUPPORTED = 0x02U,
    NOT_AUTHORIZED = 0x03U,
    COMMAND_NOT_EXECUTED = 0x04U,
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
    TRANSPORT_INTEGRITY_CHECK_ERROR = 0,
    COMMAND_TOO_LONG = 1,
    COMMAND_TOO_SHORT = 2,
    SEQUENCE_NUMBER_INVALID = 3,
    MAX_CMD_NOT_EXECUTED_ERROR_CAUSE = 4
}cmd_not_executed_cause_t;

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
    uint8_t *buf;
}mdfu_packet_t;

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
    uint16_t cmd_timeouts[MAX_MDFU_CMD];
    uint32_t inter_transaction_delay;
}client_info_t;

int mdfu_init(transport_t *transport, int retries);
int mdfu_open(void);
int mdfu_close(void);
int mdfu_get_client_info(client_info_t *client_info);
void print_client_info(const client_info_t *client_info);
int mdfu_run_update(const image_reader_t *image_reader);
#endif