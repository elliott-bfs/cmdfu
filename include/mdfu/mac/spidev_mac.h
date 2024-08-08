#ifndef SPIDEV_MAC_H
#define SPIDEV_MAC_H

#include "mac.h"

struct spidev_config {
    uint8_t mode;
    uint8_t bits_per_word;
    uint32_t speed;
    char *path;
};

void get_spidev_mac(mac_t **mac);

#endif