#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h> // close()
#include <fcntl.h>
#include <termios.h>
#include <strings.h> // bzero()
#include <stdbool.h>
#include <sys/types.h>
#include <sys/time.h>
#include "mdfu/mac/serial_mac.h"
#include "mdfu/logging.h"

#define PORT_NAME_MAX_SIZE 256

static bool opened = false;
static char port[PORT_NAME_MAX_SIZE + 1];
static int serial_port = 0;
static int baudrate = 0;

int get_baudrate(int baud)
{
    switch (baud) {
    case 9600:
        return B9600;
    case 19200:
        return B19200;
    case 38400:
        return B38400;
    case 57600:
        return B57600;
    case 115200:
        return B115200;
    case 230400:
        return B230400;
    case 460800:
        return B460800;
    case 500000:
        return B500000;
    case 576000:
        return B576000;
    case 921600:
        return B921600;
    case 1000000:
        return B1000000;
    case 1152000:
        return B1152000;
    case 1500000:
        return B1500000;
    case 2000000:
        return B2000000;
    case 2500000:
        return B2500000;
    case 3000000:
        return B3000000;
    case 3500000:
        return B3500000;
    case 4000000:
        return B4000000;
    default: 
        return -1;
    }
}

int mac_init(void *conf)
{
    struct serial_config *config = (struct serial_config *) conf;
    if(opened){
        ERROR("Cannot initialize while MAC is opened.");
        errno = EBUSY;
        return -1;
    }
    DEBUG("Initializing serial MAC");
    int port_name_size = strlen(config->port);
    if(PORT_NAME_MAX_SIZE < port_name_size){
        ERROR("This driver only supports serial port name lenght of max 256 characters");
        errno = EINVAL;
        return -1;
    }
    strcpy(port, config->port);
    baudrate = config->baudrate;
    return 0;
}

int mac_open(void)
{
    struct termios tty;
    DEBUG("Opening serial MAC");
    if(opened){
        errno = EBUSY;
        return -1;
    }

    serial_port = open(port, O_RDWR);

    if(serial_port < 0){
        ERROR("open: %s", strerror(errno));
        return -1;
    }

    if(tcgetattr(serial_port, &tty) != 0){
        ERROR("tcgetattr: %s", strerror(errno));
        return -1;
    }
    tty.c_cflag &= ~PARENB; // clear parity bit
    tty.c_cflag &= ~CSTOPB; // one stop bit
    tty.c_cflag &= ~CSIZE; // clear, then set 8 bits per byte
    tty.c_cflag |= CS8;
    //tty.c_cflag &= ~CRTSCTS; // hardware flow control - seems not support on my Linux distro
    tty.c_cflag |= CREAD | CLOCAL; // enable read and ignore ctrl lines
    
    tty.c_lflag &= ~ECHO; // Disable echo
    tty.c_lflag &= ~ECHOE; // Disable erasure
    tty.c_lflag &= ~ECHONL; // Disable new-line echo
    tty.c_lflag &= ~ICANON; // raw data mode - no processing (non-canonical mode)
    tty.c_lflag &= ~ISIG; // Disable interpretation of INTR, QUIT and SUSP
    
    tty.c_iflag &= ~(IXON | IXOFF | IXANY); // Turn off s/w flow ctrl
    tty.c_iflag &= ~(IGNBRK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL); // Disable any special handling of received bytes
    
    tty.c_oflag &= ~OPOST; // Prevent special interpretation of output bytes (e.g. newline chars)
    tty.c_oflag &= ~ONLCR; // Prevent conversion of newline to carriage return/line feed

    tty.c_cc[VTIME] = 10;    // Wait for up to 1s (10 deciseconds), returning as soon as any data is received.
    tty.c_cc[VMIN] = 0;

    int speed = get_baudrate(baudrate);
    if( speed < 0){
        ERROR("Non standard baudrate not supported");
        errno = EINVAL;
        return -1;
    };

    cfsetispeed(&tty, speed);
    cfsetospeed(&tty, speed);

    // Save tty settings, also checking for error
    if (tcsetattr(serial_port, TCSANOW, &tty) != 0) {
        ERROR("tcsetattr: %s", strerror(errno));
        return -1;
    }
    opened = true;
    return 0;
}

int mac_close(void)
{
    DEBUG("Closing serial MAC");
    if(opened){
        close(serial_port);
        opened = false;
        return 0;
    } else {
        errno = EBADF;
        return -1;
    }
}

int mac_read(int size, uint8_t *data)
{
    int status;
    status = read(serial_port, data, size);
    if(status < 0){
        ERROR("Serial MAC read: %s", strerror(errno));
        return -1;
    }
    return status;
}

int mac_write(int size, uint8_t *data)
{
    ssize_t status;
    status = write(serial_port, data, size);
    if(status < 0){
        ERROR("Serial MAC write: %s", strerror(errno));
    }
    return status;
}

mac_t serial_mac = {
    .open = mac_open,
    .close = mac_close,
    .init = mac_init,
    .write = mac_write,
    .read = mac_read
};


void get_serial_mac(mac_t **mac){
    *mac = &serial_mac;
}
