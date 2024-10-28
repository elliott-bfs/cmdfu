#ifndef I2C_TRANSPORT_H
#define I2C_TRANSPORT_H

#include "mdfu/mac/mac.h"
#include "mdfu/transport/transport.h"

int get_i2c_transport(transport_t **transport);

#endif