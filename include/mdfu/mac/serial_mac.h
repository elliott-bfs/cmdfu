#ifndef SERIAL_MAC_H
#define SERIAL_MAC_H

#include "mac.h"

struct serial_config {
    char * port;
    int baudrate;
};

void get_serial_mac(mac_t **mac);

#endif