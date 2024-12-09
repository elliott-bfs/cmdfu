#ifndef MAC_FUNCTIONS_H
#define MAC_FUNCTIONS_H

#include <stdint.h>

int mac_open(void);
int mac_close(void);
int mac_read(int size, uint8_t *data);
int mac_write(int size, uint8_t *data);
int mac_init(void *);

#endif // MAC_FUNCTIONS_H