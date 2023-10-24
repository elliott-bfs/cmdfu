#ifndef SERIAL_TRANSPORT_H
#define SERIAL_TRANSPORT_H

#include "mac.h"

typedef struct transport {
    int (* init)(mac_t *, int timeout);
    int (* open)(void);
    int (* close)(void);
    int (* read)(int *, uint8_t *);
    int (* write)(int, uint8_t *);
} transport_t;

int get_serial_transport(transport_t * transport);

#endif