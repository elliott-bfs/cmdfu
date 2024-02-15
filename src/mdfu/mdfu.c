#include <stdio.h>
#include <errno.h>
#include "mdfu/mdfu.h"
#include "mdfu/transport.h"

static uint16_t chunk_size;
static transport_t transport;
static uint8_t sequence_number;
static int send_retries;

int send_cmd(mdfu_packet_t *mdfu_cmd_packet);

static inline void increment_sequence_number(){
    sequence_number = (sequence_number + 1) & 0x1F;
}

void log_packet(mdfu_packet_t *packet, mdfu_packet_type_t type){
    
    if(type == MDFU_CMD){
        printf("Sequence number %d; Command %s; Sync %s; Data size %d",
            packet->sequence_number,
            MDFU_COMMANDS_STR[packet->command],
            packet->sync ? "true" : "false",
            packet->data_length);
    } else {
        printf("Sequence number %d; Status %s; Resend %s; Data size %d",
            packet->sequence_number,
            MDFU_STATUS_STR[packet->status],
            packet->resend ? "true" : "false",
            packet->data_length);
    }
    if(packet->data_length) {
        for(int i = 0; i < packet->data_length; i++){
            printf("%02x", packet->data[i]);
        }
        printf("\n");
    }
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
    transport = transport;
    sequence_number = 0;
    send_retries = retries;

}

// TODO it would be better to send in a struct containing functions for reading chunks
// this would be a good abstraction because then we can offer multiple ways to
// read the image file piecewise instead of everything at once
int mdfu_run_upgrade(uint8_t *image, int image_size){
    mdfu_packet_t mdfu_packet;

    if(transport.open() < 0){
        perror("MDFU update failed: ");
    }
    
    mdfu_packet.command = GET_TARGET_BUFFER_INFO;
    mdfu_packet.data = NULL;
    mdfu_packet.data_length = 0;
    mdfu_packet.sync = true;
    if(send_cmd(&mdfu_packet) < 0){
        transport.close();
        return -1;
    };
    transport.close();
}

int send_cmd(mdfu_packet_t *mdfu_cmd_packet){
    mdfu_packet_buffer_t packet_buffer;
    mdfu_packet_buffer_t rx_packet_buffer;
    mdfu_packet_t mdfu_status_packet;
    int status;
    int retries = send_retries;

    mdfu_encode_cmd_packet(mdfu_cmd_packet, &packet_buffer);
    
    printf("Sending MDFU command packet\n");
    log_packet(mdfu_cmd_packet, MDFU_CMD);

    while(retries){
        retries -= 1;
        status = transport.write(packet_buffer.size, packet_buffer.buffer);
        if(status < 0){
            break;
        }
        status = transport.read(&rx_packet_buffer.size, rx_packet_buffer.buffer);
        if(status < 0){
            break;
        }
        mdfu_decode_packet(&mdfu_status_packet, MDFU_STATUS, &rx_packet_buffer); 
        printf("Received MDFU status packet\n");
        log_packet(&mdfu_status_packet, MDFU_STATUS);

        if(mdfu_status_packet.resend){
            // debug logging here
            continue;
        } else if(mdfu_status_packet.status == SUCCESS) {
            increment_sequence_number();
            break;
        } else {
            increment_sequence_number();
            // error logging here
            status = -EPROTO;          
        }
    }
    if(retries == 0){
        printf("Tried %d times to send command without success", retries);
        status = -EIO;
    }
    return status;
}

int get_target_buffer_info(bool sync){

}

void get_next_chunk(uint8_t *buffer, int size){

}