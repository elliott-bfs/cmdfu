#include <errno.h>
#include "mdfu/transport.h"
#include "mdfu/serial_transport.h"

int get_transport(transport_type_t type, struct transport *transport){
    
    switch(type){
        case SERIAL_TRANSPORT:
            get_serial_transport(transport);
            break;
        default:
            errno = EINVAL;
            return -EINVAL;
    }
    return 0;

}