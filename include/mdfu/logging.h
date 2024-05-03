#ifndef LOGGING_H
#define LOGGING_H

#include <stdio.h>

extern int debug_level;
extern FILE *dbgstream;
extern const char* ERROR_LEVEL_NAMES[];
/*
The do and while (0) are a kludge to make it possible to write
logger(level, format, ...);
which the resemblance of logger() to a function would make C programmers want to do; see Swallowing the Semicolon. 
*/
#ifdef __GNUC__

/*This does not work for C99 because if no arguments are given there will still be a comma
    logger(level, format, )
  */
 #if 0
#define logger(level, format, ...) do {  \
    if (level <= debug_level) { \
        fprintf(dbgstream,"%s:%d:" format "\n", __FILE__, __LINE__, ## __VA_ARGS__); \
    } \
} while (0)
#else
#define logger(level, format, ...) do {  \
    if (level <= debug_level) { \
        fprintf(dbgstream,"%s:" format "\n", ERROR_LEVEL_NAMES[level], ## __VA_ARGS__); \
    } \
} while (0)
#endif

#else
#define logger(level, ...) do {  \
    if (level <= debug_level) { \
        fprintf(dbgstream,"%s:%d:", __FILE__, __LINE__); \
        fprintf(dbgstream,__VA_ARGS__); \
        fprintf(dbgstream,"\n"); \
    } \
} while (0)

#endif

#define ERRORLEVEL 1
#define WARNLEVEL  2
#define INFOLEVEL  3
#define DEBUGLEVEL 4

#define LOG(level, format, ...) logger(level, format, ## __VA_ARGS__)
#define ERROR(format, ...) logger(ERRORLEVEL, format, ##  __VA_ARGS__)
#define WARN(format, ...) logger(WARNLEVEL, format, ## __VA_ARGS__)
#define INFO(format, ...) logger(INFOLEVEL, format, ## __VA_ARGS__)
#define DEBUG(format, ...) logger(DEBUGLEVEL, format, ## __VA_ARGS__)

#if 0
#define LOG(level, format, ...) do { \
    if(level == LOG_ERR) {\
        ERROR(format, __VA_ARGS__)\
    }\
} while(0)

#define ERROR(format, arg...) \
	swupdate_notify(FAILURE, format, ERRORLEVEL, ## arg)

#define WARN(format, arg...) \
	swupdate_notify(RUN, format, WARNLEVEL, ## arg)

#define INFO(format, arg...) \
	swupdate_notify(RUN, format, INFOLEVEL, ## arg)

#define TRACE(format, arg...) \
	swupdate_notify(RUN, format, TRACELEVEL, ## arg)

#define DEBUG(format, arg...) \
	swupdate_notify(RUN, format, DEBUGLEVEL, ## arg)
#endif
void init_logging(FILE *logstream);
void set_debug_level(int level);

#endif