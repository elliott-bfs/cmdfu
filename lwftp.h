#ifndef LWFTP_H
#define LWFTP_H
#include <stdint.h>
#include <stdbool.h>

#define LWFTP_COMMAND_SIZE 1
#define LWFTP_SEQUENCE_FIELD_SIZE 1
#define LWFTP_DATA_MAX_SIZE 1024
#define LWFTP_PACKET_BUFFER_SIZE (LWFTP_SEQUENCE_FIELD_SIZE + LWFTP_COMMAND_SIZE + LWFTP_DATA_MAX_SIZE)

typedef enum
{
    GET_TRANSFER_PARAMETERS = 0x01U,
    START_TRANSFER = 0x02U,
    WRITE_CHUNK = 0x03U,
    GET_IMAGE_STATE = 0x04U,
    END_TRANSFER = 0x05U
} lwftp_command_t;

typedef enum
{
    COMMAND_SUCCESS = 0x01U,
    COMMAND_NOT_SUPPORTED = 0x02U,
    COMMAND_NOT_AUTHORIZED = 0x03U,
    TRANSPORT_FAILURE = 0x04U,
    ABORT_TRANSFER = 0x05U
} lwftp_response_status_t;



typedef struct __attribute__((packed))
{
    uint16_t maxPayloadSize;
    uint8_t numberOfPacketBuffers;
} ftp_discovery_data_t;

static uint16_t chunk_size;

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
    uint8_t data[LWFTP_DATA_MAX_SIZE];
}lwftp_packet_t;

struct packet_buffer {
    uint8_t size;
    uint8_t buffer[LWFTP_PACKET_BUFFER_SIZE];
};
#endif