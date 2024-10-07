#ifndef I2CDEV_MAC_H
#define I2CDEV_MAC_H

#include "mac.h"

struct i2cdev_config {
    int address;
    char *path;
};

void get_i2cdev_mac(mac_t **mac);

#endif