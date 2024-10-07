#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <unistd.h> // close()
#include <fcntl.h>
#include <sys/ioctl.h>
#include <stdbool.h>
#include <linux/spi/spidev.h>
#include "mdfu_config.h"
#include "mdfu/mac/spidev_mac.h"
#include "mdfu/logging.h"

#define PATH_NAME_MAX_SIZE 256

static char rx_buffer[MDFU_MAX_COMMAND_DATA_LENGTH];

typedef struct {
    int fd;
    uint8_t mode;
    uint8_t bits_per_word;
    uint32_t speed;
    char path[PATH_NAME_MAX_SIZE];
    char *rx_buffer;
    int rx_data_length;
} spi_device_t;

static spi_device_t device = {
    .fd = -1,
    .rx_buffer = rx_buffer,
    .rx_data_length = 0
};


/**
 * @brief Initializes the MAC with the given configuration.
 *
 * This function initializes the MAC by setting up the SPI device with the provided configuration.
 * It checks if the device is already opened and ensures the path name length is within the allowed limit.
 *
 * @param conf A pointer to a `spidev_config` structure containing the configuration parameters.
 * 
 * @return 0 on success, -1 on failure with `errno` set appropriately.
 * 
 * @retval 0 Initialization was successful.
 * @retval -1 Initialization failed. Possible reasons include:
 *         - The device is already opened (`errno` is set to `EBUSY`).
 *         - The path name length exceeds the maximum allowed size (`errno` is set to `EINVAL`).
 *
 * @note The function sets the following `errno` values on failure:
 *       - `EBUSY`: The device is already opened.
 *       - `EINVAL`: The path name length exceeds the maximum allowed size.
 */
static int mac_init(void *conf)
{
    struct spidev_config *config = (struct spidev_config *) conf;
    if(device.fd != -1){
        ERROR("Cannot initialize while MAC is opened.");
        errno = EBUSY;
        return -1;
    }
    DEBUG("Initializing spidev MAC");
    int port_name_size = strlen(config->path);
    if(PATH_NAME_MAX_SIZE < port_name_size){
        ERROR("This driver only supports path name lenght of max 256 characters");
        errno = EINVAL;
        return -1;
    }
    strncpy(device.path, config->path, PATH_NAME_MAX_SIZE);
    device.mode = config->mode;
    device.bits_per_word = config->bits_per_word;
    device.speed = config->speed;
    return 0;
}

static int mac_open(void)
{
    DEBUG("Opening spidev MAC");
    if(device.fd != -1){
        errno = EBUSY;
        return -1;
    }

    device.fd = open(device.path, O_RDWR);
    if (device.fd < 0) {
        perror("Failed to open SPI device");
        return -1;
    }

    if (ioctl(device.fd, SPI_IOC_WR_MODE, &device.mode) < 0 ||
        ioctl(device.fd, SPI_IOC_WR_BITS_PER_WORD, &device.bits_per_word) < 0 ||
        ioctl(device.fd, SPI_IOC_WR_MAX_SPEED_HZ, &device.speed) < 0) {
        perror("Failed to set SPI parameters");
        close(device.fd);
        device.fd = -1;
        return -1;
    }
    return 0;
}

static int mac_close(void)
{
    DEBUG("Closing SPI MAC");
    if(device.fd != -1){
        close(device.fd);
        device.fd = -1;
        return 0;
    } else {
        errno = EBADF;
        return -1;
    }
}


static int spi_transfer(uint8_t *tx_buffer, uint8_t *rx_buffer, size_t length) {
    if (device.fd < 0 || !tx_buffer || !rx_buffer || length == 0) return -1;
    assert(device.fd >= 0);

    struct spi_ioc_transfer transfer = {
        .tx_buf = (unsigned long)tx_buffer,
        .rx_buf = (unsigned long)rx_buffer,
        .len = length,
        .speed_hz = device.speed,
        .bits_per_word = device.bits_per_word,
    };

    if (ioctl(device.fd, SPI_IOC_MESSAGE(1), &transfer) < 0) {
        ERROR("Failed to perform SPI transfer: %s", strerror(errno));
        return -1;
    }

    return 0;
}

static int mac_read(int size, uint8_t *data)
{
    int status;
    
    if(device.rx_data_length != size){
        ERROR("spidev MAC read size must match last write size: %s", strerror(errno));
        ERROR("Requested size was %d and buffer contains %d", size, device.rx_data_length);
        return -1;
    }

    memcpy(data, device.rx_buffer, size);
    device.rx_data_length = 0;
    return 0;
}

static int mac_write(int size, uint8_t *data)
{
    if(spi_transfer(data, device.rx_buffer, size) < 0){
        ERROR("Serial MAC write: %s", strerror(errno));
        return -1;
    }
    device.rx_data_length = size;
    return 0;
}

mac_t spidev_mac = {
    .open = mac_open,
    .close = mac_close,
    .init = mac_init,
    .write = mac_write,
    .read = mac_read
};


void get_spidev_mac(mac_t **mac){
    *mac = &spidev_mac;
}
