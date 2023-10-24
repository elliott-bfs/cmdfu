#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h> // close()
#include <strings.h> // bzero()
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h> // inet_pton
#include "socket_mac.h"


static int sock = 0;
static struct sockaddr_in socket_address;
/** Blocking socket implementation
 * 
*/

/* Blocking vs non blocking

What are the Basic Programming Techniques for Dealing with Blocking Sockets?

    Have a design that doesn't care about blocking
    Using select
    Using non-blocking sockets.
    Using multithreading or multitasking

Options to enable non-blocking socket
- SOCK_NONBLOCK flag in socket() function
- ioctl/fctl (ioctlsocket or similar on Windows)
- MSG_DONTWAIT flag in send/recv functions
- select/poll

It is sometimes convenient to employ the "send/recv" family of system calls. If the flags parameter contains the MSG_DONTWAIT flag, each call will behave similar to a socket having the O_NONBLOCK flag set

fcntl() or ioctl() are used to set the properties for file streams. When you use this function to make a socket non-blocking, function like accept(), recv() and etc, which are blocking in nature will return error and errno would be set to EWOULDBLOCK. You can poll file descriptor sets to poll on sockets

#include <fcntl.h>

//Returns true on success, or false if there was an error
bool SetSocketBlockingEnabled(int fd, bool blocking)
{
   if (fd < 0) return false;

#ifdef _WIN32
   unsigned long mode = blocking ? 0 : 1;
   return (ioctlsocket(fd, FIONBIO, &mode) == 0) ? true : false;
#else
   int flags = fcntl(fd, F_GETFL, 0);
   if (flags == -1) return false;
   flags = blocking ? (flags & ~O_NONBLOCK) : (flags | O_NONBLOCK);
   return (fcntl(fd, F_SETFL, flags) == 0) ? true : false;
#endif
}
*/

int mac_init(void *conf)
{
    struct socket_config *config = (struct socket_config *) conf;
    struct timeval timeout = {
        .tv_sec = 5,
        .tv_usec = 0
    };

    sock = socket(PF_INET, SOCK_STREAM, 0);
    if(sock < 0) {
		perror("Socket MAC init");
		return -1;
	}
    
    if(setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (void *) &timeout, sizeof(timeout)) < 0){
        perror("Socket MAC init");
        return -1;
    }

    bzero(&socket_address, sizeof(socket_address));
    inet_pton(AF_INET, config->host, &(socket_address.sin_addr));
    socket_address.sin_family = AF_INET;
    //printf("%d", config->port);
    socket_address.sin_port = htons(config->port);
    return 0;
}

int mac_open(void)
{
    if(connect(sock, (struct sockaddr *) &socket_address, sizeof(socket_address)) < 0){
        perror("Socket MAC connect");
        close(sock);
        return -1;
    }
    return 0;
}

int mac_close(void)
{
    close(sock);
    return 0;
}

int mac_read(int size, uint8_t *data)
{
    int status;

    status = recv(sock, data, size, 0);
    if(status < 0){
        perror("Socket MAC read");
    }
    return status;
}

int mac_write(int size, uint8_t *data)
{
    int status;
    status = send(sock, data, size, 0);
    if(status < 0){
        perror("Socket MAC send:");
    }
    return status;
}

void get_socket_mac(struct mac * mac){
    mac->init = mac_init;
    mac->open = mac_open;
    mac->close = mac_close;
    mac->read = mac_read;
    mac->write = mac_write;
}