#include <errno.h>
#include "mdfu/transport/transport.h"
#include "mdfu/transport/serial_transport.h"
#include "mdfu/transport/spi_transport.h"
#include "mdfu/transport/i2c_transport.h"

int get_transport(transport_type_t type, transport_t **transport){
    
    switch(type){
        case SERIAL_TRANSPORT:
            get_serial_transport(transport);
            break;
        case SERIAL_TRANSPORT_BUFFERED:
            get_serial_transport_buffered(transport);
            break;
        case SPI_TRANSPORT:
            get_spi_transport(transport);
            break;
        case I2C_TRANSPORT:
            get_i2c_transport(transport);
            break;
        default:
            errno = EINVAL;
            return -EINVAL;
    }
    return 0;
}