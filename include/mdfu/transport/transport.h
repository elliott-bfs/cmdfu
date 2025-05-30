#ifndef TRANSPORT_H
#define TRANSPORT_H

#include <stdint.h>
#include "mdfu/mac/mac.h"

typedef enum transport_type{
    SERIAL_TRANSPORT,
    SERIAL_TRANSPORT_BUFFERED,
    SPI_TRANSPORT,
    SOCKET_TRANSPORT,
    I2C_TRANSPORT
} transport_type_t;

// IOCTL argument is a float and represents seconds
#define TRANSPORT_IOC_INTER_TRANSACTION_DELAY 1

typedef struct transport {
    int (* init)(mac_t *, int timeout);
    int (* open)(void);
    int (* close)(void);
    int (* read)(int *, uint8_t *, float);
    int (* write)(int, uint8_t *);
    int (* ioctl)(int, ...);
} transport_t;

int get_transport(transport_type_t type, transport_t **transport);

#endif