#include <errno.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include "mdfu/timeout.h"

int set_timeout(timeout_t *timer, float timeout){
    long nsec;
    time_t seconds;
    if (clock_gettime(CLOCK_MONOTONIC, timer) == -1) {
        perror("clock_gettime");
        return -1;
    }
    seconds = (time_t) timeout;
    nsec = (timeout - seconds) * 1e9;

    timer->tv_sec = timer->tv_sec + seconds;
    timer->tv_nsec = timer->tv_nsec + nsec;
    if (timer->tv_nsec >= 1e9) {
        timer->tv_sec += 1;
        timer->tv_nsec -= 1e9;
    }
    return 0;
}

bool timeout_expired(timeout_t *timer){
    timeout_t now;
    bool expired = false;

    if (clock_gettime(CLOCK_MONOTONIC, &now) == -1) {
        perror("clock_gettime");
    }
    if(now.tv_sec > timer->tv_sec) {
        expired = true;
    }else if(now.tv_sec == timer->tv_sec && now.tv_nsec > timer->tv_nsec){
        expired = true;
    }
    return expired;
}