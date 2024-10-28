#ifndef TIMEOUT_H
#define TIMEOUT_H
#include <time.h>

typedef struct timespec timeout_t;


int set_timeout(timeout_t *timer, float timeout);
bool timeout_expired(timeout_t *timer);

#endif