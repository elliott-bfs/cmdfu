#ifndef TRANSPORT_H
#define TRANSPORT_H

#include <stdint.h>
#include "mdfu/mac/mac.h"

typedef enum transport_type{
    SERIAL_TRANSPORT,
    SPI_TRANSPORT,
    SOCKET_TRANSPORT,
    I2C_TRANSPORT
} transport_type_t;

typedef struct transport {
    int (* init)(mac_t *, int timeout);
    int (* open)(void);
    int (* close)(void);
    int (* read)(int *, uint8_t *, float);
    int (* write)(int, uint8_t *);
} transport_t;

int get_transport(transport_type_t type, transport_t **transport);

#endif