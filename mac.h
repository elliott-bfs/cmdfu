#ifndef MAC_H
#define MAC_H

#include <stdint.h>


typedef struct mac {
    int (* init)(void *);
    int (* open)(void);
    int (* close)(void);
    int (* read)(int, uint8_t *);
    int (* write)(int, uint8_t *);
} mac_t;

#endif