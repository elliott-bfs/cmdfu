#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h> // close()
#include <strings.h> // bzero()
#include <stdbool.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>

//#include "mdfu_config.h"
#include "mdfu/mac/mac.h"
#include "mdfu/logging.h"
#include "mdfu/mac/i2cdev_mac.h"

#define PATH_NAME_MAX_SIZE 256

typedef struct {
    int fd;
    unsigned long address;
    char path[PATH_NAME_MAX_SIZE];
} i2c_device_t;

static i2c_device_t device = {
    .fd = -1,
};

int mac_init(void *conf)
{
    struct i2cdev_config *config = (struct i2cdev_config *) conf;
    if(device.fd != -1){
        ERROR("Cannot initialize while MAC is opened.");
        errno = EBUSY;
        return -1;
    }
    DEBUG("Initializing i2cdev MAC");
    int port_name_size = strlen(config->path);
    if(PATH_NAME_MAX_SIZE < port_name_size){
        ERROR("This driver only supports path name lenght of max 256 characters");
        errno = EINVAL;
        return -1;
    }
    strncpy(device.path, config->path, PATH_NAME_MAX_SIZE);
    device.address = config->address;
    return 0;
}

int mac_open(void)
{
    DEBUG("Opening i2cdev MAC");
    if(device.fd != -1){
        errno = EBUSY;
        return -1;
    }

    device.fd = open(device.path, O_RDWR);
    if (device.fd < 0) {
        ERROR("Failed to open I2C device: %s", strerror(errno));
        return -1;
    }

    if (ioctl(device.fd, I2C_SLAVE, device.address) < 0 ||
        ioctl(device.fd, I2C_TIMEOUT, 10) < 0 ||
        ioctl(device.fd, I2C_RETRIES, 0) < 0) {
        ERROR("Failed to set I2C parameters %s", strerror(errno));
        close(device.fd);
        device.fd = -1;
        return -1;
    }
    return 0;
}

int mac_close(void)
{
    DEBUG("Closing i2cdev MAC");
    if(device.fd != -1){
        close(device.fd);
        device.fd = -1;
        return 0;
    } else {
        errno = EBADF;
        return -1;
    }
}

int mac_read(int size, uint8_t *data)
{
    int status;

    status = read(device.fd, data, size);
    if(status < 0){
        ERROR("i2cdev MAC read: %s", strerror(errno));
    }
    return status;
}

int mac_write(int size, uint8_t *data)
{
    int status;

    status = write(device.fd, data, size);
    if(status < 0){
        ERROR("i2cdev MAC write: %s", strerror(errno));
    }
    return status;
}

mac_t i2cdev_mac = {
    .open = mac_open,
    .close = mac_close,
    .init = mac_init,
    .write = mac_write,
    .read = mac_read
};

void get_i2cdev_mac(mac_t **mac){
    *mac = &i2cdev_mac;
}
