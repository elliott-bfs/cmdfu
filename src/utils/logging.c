#include "mdfu/logging.h"

FILE *dbgstream;
int  debug_level = ERRORLEVEL;

void init_logging(FILE *logstream){
    if(logstream){
        dbgstream = logstream;
    } else {
        dbgstream = stdout;
    }
}
void set_debug_level(int level){
    if(0 < level && level < 6)
    {
        debug_level = level;
    }
    else {
        LOG(ERRORLEVEL, "Debug level must be between 1 and 5");
        ERROR("Bla bla %d ", 11);
    }
}