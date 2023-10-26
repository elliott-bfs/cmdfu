#include "mdfu.h"

#define MDFU_HEADER_SYNC 0x80
#define MDFU_HEADER_RESEND 0x40
#define MDFU_HEADER_SEQUENCE_NUMBER 0x1F

void encode_packet(mdfu_packet_t *packet, bool cmd, uint8_t *packet_buffer){
    uint8_t sequence_field = 0;

    if(packet->sync){
        sequence_field |= MDFU_HEADER_SYNC;
    }
    packet_buffer[0] = sequence_field;
    packet_buffer[1] = packet->command;
    
    for(uint16_t i=0; i < packet->data_length; i++)
    {
        packet_buffer[i+2] = packet->data[i];
    }
}

void decode_packet(mdfu_packet_t *packet, bool cmd, uint8_t *packet_buffer){
    if(cmd){
        packet->sync = (packet_buffer[0] & MDFU_HEADER_SYNC) ? true : false;
        packet->command = packet_buffer[1];
    }else{
        packet->resend = (packet_buffer[0] & MDFU_HEADER_RESEND) ? true : false;
        packet->status = packet_buffer[1];
    }
    packet->sequence_number = packet_buffer[0] & MDFU_HEADER_SEQUENCE_NUMBER;

}