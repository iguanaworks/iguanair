#include "base.h"
#include "pipes.h"

bool createPipePair(PIPE_PTR *pair)
{
    return false;
}

int readPipe(PIPE_PTR fd, void *buf, size_t count)
{
    return -1;
}

int writePipe(PIPE_PTR fd, const void *buf, size_t count)
{
    return -1;
}

int closePipe(PIPE_PTR fd)
{
    return -1;
}

PIPE_PTR acceptClient(PIPE_PTR *server)
{
    return INVALID_PIPE;
}

void socketName(const char *name, char *buffer, unsigned int length)
{
    
}

PIPE_PTR startListening(const char *name, const char *alias)
{
    return INVALID_PIPE;
}

void stopListening(PIPE_PTR fd, const char *name, const char *alias)
{
}

PIPE_PTR connectToPipe(const char *name)
{
    return INVALID_PIPE;
}

