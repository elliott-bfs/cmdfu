#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h> // close()
#include <strings.h> // bzero()
#include <stdbool.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h> // inet_pton
#include "mdfu/mac/socket_mac.h"
#include "mdfu/logging.h"

#define HEADER_SIZE 8
#define HEADER_MAGIC "MDFU"

static int sock = 0;
static struct sockaddr_in socket_address;
static bool opened = false;

static int mac_init(void *conf)
{
    struct socket_config *config = (struct socket_config *) conf;
    struct timeval timeout = {
        .tv_sec = 5,
        .tv_usec = 0
    };
    DEBUG("Initializing socket MAC");
    sock = socket(PF_INET, SOCK_STREAM, 0);
    if(sock < 0) {
		perror("Socket MAC init");
		return -1;
	}
    
    if(setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (void *) &timeout, sizeof(timeout)) < 0){
        perror("Socket MAC init");
        return -1;
    }

    // The send timeout will also set the connect timeout, which is very long otherwise ~75 seconds
    // Maybe we should do this differently with poll/select
    if(setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) < 0){
        perror("Socket MAC initialization failed with: ");
        return -1;
    }

    bzero(&socket_address, sizeof(socket_address));
    inet_pton(AF_INET, config->host, &(socket_address.sin_addr));
    socket_address.sin_family = AF_INET;
    socket_address.sin_port = htons(config->port);
    opened = false;
    return 0;
}

static int mac_open(void)
{
    DEBUG("Opening socket MAC");
    if(opened){
        errno = EBUSY;
        return -EBUSY;
    }
    char buf[64];
    inet_ntop (AF_INET, &socket_address.sin_addr, buf, sizeof(buf));
    DEBUG("Connecting to host %s on port %d",buf , ntohs(socket_address.sin_port));

    if(connect(sock, (struct sockaddr *) &socket_address, sizeof(socket_address)) < 0){
        if(errno == EINPROGRESS){
            ERROR("Socket MAC connect timed out");
        } else {
            ERROR("Socket MAC connect failed with: %s", strerror(errno));
        }
        close(sock);
        return -ETIMEDOUT;
    }
    opened = true;
    return 0;
}

static int mac_close(void)
{
    if(opened){
        close(sock);
        opened = false;
        return 0;
    } else {
        return -1;
    }
}

/**
 * @brief Reads a MAC frame from a socket.
 *
 * This function reads a MAC frame from a socket. It first reads the header to
 * verify the frame and then reads the actual data if the header is valid.
 *
 * @param size The expected size of the data to be read.
 * @param data A pointer to a buffer where the read data will be stored.
 * @return The number of bytes read on success, or -1 on failure.
 *
 * @details
 * The function performs the following steps:
 * 1. Reads the header from the socket.
 * 2. Verifies that the header contains the expected 'MDFU' signature.
 * 3. Extracts the frame size from the header.
 * 4. Compares the extracted frame size with the expected size.
 * 5. Reads the data from the socket if the sizes match.
 */
static int mac_read(int size, uint8_t *data)
{
    int status;
    uint8_t header[HEADER_SIZE];
    uint32_t frame_size;

    // Receive the header
    status = recv(sock, header, HEADER_SIZE, 0);
    if(status < 0){
        ERROR("MacSocketPacket: %s", strerror(errno));
        return -1;
    }
    if(memcmp(header, HEADER_MAGIC, 4) == 0){
        frame_size =  (header[7] << 25) | (header[6] << 16) | (header[5] << 8) | header[4];
        if(frame_size != size){
            ERROR("MacSocketPacket: Requested read size (%d) does not match packet size (%d)", size, frame_size);
            return -1;
        }
        // Receive the data
        status = recv(sock, data, frame_size, 0);
        if(status < 0){
            ERROR("MacSocketPacket: %s", strerror(errno));
        return -1;
    }
    }else{
        ERROR("MacSocketPacket: Received invalid frame header");
        return -1;
    } 
    return status;
}

/**
 * @brief Sends the entire buffer over a socket.
 *
 * This function attempts to send the entire buffer over the specified socket.
 * It handles partial sends and retries in case of interruptions by signals.
 *
 * @param sockfd The file descriptor of the socket.
 * @param buffer A pointer to the buffer containing the data to be sent.
 * @param length The length of the buffer in bytes.
 * @return The total number of bytes sent, or -1 if an error occurred.
 *
 */
static ssize_t send_all(int sockfd, const void *buffer, size_t length) {
    const char *ptr = (const char *)buffer;
    size_t total_sent = 0;
    ssize_t bytes_sent;

    while (total_sent < length) {
        bytes_sent = send(sockfd, ptr + total_sent, length - total_sent, 0);
        if (bytes_sent == -1) {
            if (errno == EINTR) {
                // Interrupted by a signal, retry sending
                continue;
            } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // Non-blocking mode and no data can be sent right now, retry sending
                continue;
            } else {
                // An error occurred
                ERROR("MacSocketPacket: %s", strerror(errno));
                return -1;
            }
        }
        total_sent += bytes_sent;
    }
    return total_sent;
}

/**
 * @brief Sends a data frame with a specific header over a socket.
 *
 * This function constructs a header with the format "MDFU" followed by the size of the data frame.
 * It then sends the header and the data frame over a socket.
 *
 * @param size The size of the data frame to be sent.
 * @param data A pointer to the data frame to be sent.
 * @return The number of bytes sent on success, or -1 on failure.
 *
 */
static int mac_write(int size, uint8_t *data)
{
    ssize_t status;
    uint8_t header[HEADER_SIZE] = {'M','D','F','U',0,0,0,0};
    uint32_t frame_size = size;

    header[4] = frame_size & 0xff;
    header[5] = (frame_size >> 8) & 0xff;
    header[6] = (frame_size >> 16) & 0xff;
    header[7] = (frame_size >> 24) & 0xff;

    status = send_all(sock, header, HEADER_SIZE);
    if(status < 0){
        return -1;
    }
    status = send_all(sock, data, size);
    if(status < 0){
        return -1;
    }
    return status;
}

mac_t network_packet_mac = {
    .open = mac_open,
    .close = mac_close,
    .init = mac_init,
    .write = mac_write,
    .read = mac_read
};


void get_socket_packet_mac(mac_t **mac){
    *mac = &network_packet_mac;
}
