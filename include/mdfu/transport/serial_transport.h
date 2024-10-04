#ifndef SERIAL_TRANSPORT_H
#define SERIAL_TRANSPORT_H

#include "mdfu/mac/mac.h"
#include "mdfu/transport/transport.h"

int get_serial_transport(transport_t **transport);

#endif