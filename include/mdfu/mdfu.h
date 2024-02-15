#ifndef MDFU_H
#define MDFU_H
#include <stdint.h>
#include <stdbool.h>

#define MDFU_COMMAND_SIZE 1
#define MDFU_SEQUENCE_FIELD_SIZE 1
#define MDFU_DATA_MAX_SIZE 1024
#define MDFU_PACKET_BUFFER_SIZE (MDFU_SEQUENCE_FIELD_SIZE + MDFU_COMMAND_SIZE + MDFU_DATA_MAX_SIZE)

#define MDFU_HEADER_SYNC 0x80
#define MDFU_HEADER_RESEND 0x40
#define MDFU_HEADER_SEQUENCE_NUMBER 0x1F

const char *MDFU_COMMANDS_STR[] = {
    "Get Target Buffer Info",
    "Start Transfer",
    "Write Chunk",
    "Get Image State",
    "End Transfer"
};

const char *MDFU_STATUS_STR[] = {
    "Success",
    "Command Not Supported",
    "Command Not Authorized",
    "Transfer Failure",
    "Abort File Transfer"
};

typedef enum
{
    GET_TARGET_BUFFER_INFO = 0x01U,
    START_TRANSFER = 0x02U,
    WRITE_CHUNK = 0x03U,
    GET_IMAGE_STATE = 0x04U,
    END_TRANSFER = 0x05U
} mdfu_command_t;

typedef enum
{
    SUCCESS = 0x01U,
    COMMAND_NOT_SUPPORTED = 0x02U,
    COMMAND_NOT_AUTHORIZED = 0x03U,
    TRANSPORT_FAILURE = 0x04U,
    ABORT_FILE_TRANSFER = 0x05U
} mdfu_status_t;



typedef struct __attribute__((packed))
{
    uint16_t maxPayloadSize;
    uint8_t numberOfPacketBuffers;
} mdfu_target_buffer_info_t;


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

#endif