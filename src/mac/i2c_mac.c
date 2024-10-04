#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h> // close()
#include <strings.h> // bzero()
#include <stdbool.h>

#include <sys/types.h>
#include <sys/time.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>

#include "mdfu/mac/mac.h"
#include "mdfu/logging.h"

//#include "mdfu/i2c_mac.h"



static bool opened = false;


int mac_init(void *conf)
{
    struct socket_config *config = (struct socket_config *) conf;
    struct timeval timeout = {
        .tv_sec = 5,
        .tv_usec = 0
    };
    DEBUG("Initializing i2c MAC");
    
    opened = false;
    return 0;
}

int mac_open(void)
{
    DEBUG("Opening i2c MAC");
    if(opened){
        errno = EBUSY;
        return -1;
    }
    
    opened = true;
    return 0;
}

int mac_close(void)
{
    if(opened){

        opened = false;
        return 0;
    } else {
        return -1;
    }
}

int mac_read(int size, uint8_t *data)
{
    int status;

    if(status < 0){
        perror("Socket MAC read");
    }
    return status;
}

int mac_write(int size, uint8_t *data)
{
    int status;

    if(status < 0){
        perror("Socket MAC send:");
    }
    return status;
}

mac_t network_mac = {
    .open = mac_open,
    .close = mac_close,
    .init = mac_init,
    .write = mac_write,
    .read = mac_read
};


void get_socket_mac(mac_t **mac){
    *mac = &network_mac;
}
