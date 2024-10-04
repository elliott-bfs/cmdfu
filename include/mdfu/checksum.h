#ifndef CHECKSUM_H
#define CHECKSUM_H

#include <stdint.h>

uint16_t calculate_crc16(int size, uint8_t *data);

#endif