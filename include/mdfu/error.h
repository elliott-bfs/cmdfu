#ifndef ERROR_H
#define ERROR_H

#define TIMEOUT_ERROR 1
#define NACK_ERROR 2
#define CHECKSUM_ERROR 3

#ifndef EIO
#define EIO 5
#endif

#ifndef EPROTO
#define EPROTO 71
#endif

#ifndef ETIMEDOUT
#define ETIMEDOUT 145
#endif

#ifndef ENOBUFS
#define ENOBUFS 132
#endif

#endif