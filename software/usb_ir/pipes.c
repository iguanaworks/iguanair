PIPE_PTR connectToPipe(const char *name)
{
    PIPE_PTR retval = INVALID_PIPE;
    struct sockaddr_un server;

    /* generate the server address */
    server.sun_family = PF_UNIX;
    socketName(name, server.sun_path, sizeof(server.sun_path));

    /* connect to the server */
    retval = socket(PF_UNIX, SOCK_STREAM, 0);
    if (retval != INVALID_PIPE)
    {
        if (connect(retval,
                    (struct sockaddr *)&server,
                    sizeof(struct sockaddr_un)) == -1)
        {
            closePipe(retval);
            retval = INVALID_PIPE;
        }
    }

    return retval;
}

static int readAnyDataBlock(PIPE_PTR *fds, int count, int timeout, int terminator,
                            int *index, char *buffer, int size)
{
    int retval = -1, x, max = -1;
    fd_set fdsin, fdserr;
    struct timeval tv = {0,0}, *tvp = NULL;

    /* configure the timeout if there was one*/
    if (timeout >= 0)
    {
        tv.tv_usec = timeout * 1000;
        tvp = &tv;
    }

    /* prepare the fdsets */
    FD_ZERO(&fdsin);
    for(x = 0; x < count; x++)
    {
        FD_SET(fds[x], &fdsin);
        if (fds[x] > max)
            max = fds[x];
    }
    fdserr = fdsin;

    /* select and check the return value */
    switch(select(max + 1, &fdsin, NULL, &fdserr, tvp))
    {
    case 0:
        retval = 0;
    case -1:
        if (index != NULL)
            *index = -1;
        break;

    default:
        for(x = 0; x < count; x++)
            if (FD_ISSET(fds[x], &fdserr) ||
                FD_ISSET(fds[x], &fdsin))
            {
                if (index != NULL)
                    *index = x;

                if (FD_ISSET(fds[x], &fdsin))
                {
                    /* try to read all at once */
                    int goal = 1;
                    if (terminator == NO_TERMINATOR)
                        goal = size;

                    retval = 0;
                    tv.tv_sec = 0;
                    tv.tv_usec = 0;
                    while(goal > 0)
                    {
                        int amount = readPipe(fds[x], buffer + retval, goal);
                        switch(amount)
                        {
                        case -1:
                        case 0:
                            /* break out on error or EOF */
                            goal = 0;
                            break;

                        default:
                            retval += amount;
                            /* if there is no terminator,
                               subtract from goal */
                            if (terminator == NO_TERMINATOR)
                                goal -= amount;
                            /* otherwise check for the terminator */
                            else if (buffer[retval - 1] == terminator)
                                goal = 0;
                        }
                    }
                }
                break;
            }
        break;
    }

    return retval;
}

int readBytes(PIPE_PTR fd, int timeout,
              char *buffer, int size)
{
    return readAnyDataBlock(&fd, 1, timeout, NO_TERMINATOR,
                            NULL, buffer, size);
}

int notified(PIPE_PTR fd, int timeout)
{
    char byte;
    return readAnyDataBlock(&fd, 1, timeout, NO_TERMINATOR,
                            NULL, &byte, 1);
}

bool notify(PIPE_PTR fd)
{
    char byte = '\n';
    return writePipe(fd, &byte, 1) == 1;
}

int checkFD(PIPE_PTR fd, fdSets *fds)
{
    int retval = 0;

    if (FD_ISSET(fd, &fds->err))
        retval = -1;
    else
    {
        if (FD_ISSET(fd, &fds->in))
            retval = 1;

        FD_SET(fd, &fds->next);
        if (fds->max < fd)
            fds->max = fd;
    }

    return retval;
}
