#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/select.h>
#include <unistd.h>

static int  handle_fd_read(int fd, fd_set *readfds);
static int  wait_for_input(int *fds, int nfds, fd_set *readfds);
static void process_fds(int *fds, int nfds, fd_set *readfds, bool *done);
static void monitor_fds(int file_fd, int fifo_fd, int *ret);

int main(void)
{
    int file_fd;
    int fifo_fd;
    int ret;

    fifo_fd = -1;
    ret     = 0;

    file_fd = open("file.txt", O_RDONLY);

    if(file_fd == -1)
    {
        perror("open file");
        ret = 1;
        goto cleanup;
    }

    fifo_fd = open("myfifo", O_RDONLY | O_NONBLOCK);

    if(fifo_fd == -1)
    {
        perror("open fifo");
        ret = 1;
        goto cleanup;
    }

    printf("Waiting for input from stdin, file.txt, or myfifo...\n");
    monitor_fds(file_fd, fifo_fd, &ret);

cleanup:
    if(fifo_fd != -1)
    {
        close(fifo_fd);
    }

    if(file_fd != -1)
    {
        close(file_fd);
    }

    return ret;
}

static void monitor_fds(int file_fd, int fifo_fd, int *ret)
{
    fd_set readfds;
    bool   done;
    int    fds[3];
    int    nfds;

    done   = false;
    fds[0] = STDIN_FILENO;
    fds[1] = file_fd;
    fds[2] = fifo_fd;
    nfds   = 3;

    while(!done)
    {
        int ready;

        ready = wait_for_input(fds, nfds, &readfds);

        if(ready == -1)
        {
            *ret = 1;
            done = true;
        }
        else if(ready == 0)
        {
            printf("Timeout: No data within 10 seconds.\n");
            done = true;
        }
        else
        {
            process_fds(fds, nfds, &readfds, &done);
        }
    }
}

static int wait_for_input(int *fds, int nfds, fd_set *readfds)
{
    int            max_fd;
    struct timeval timeout;
    int            result;

    FD_ZERO(readfds);

    max_fd = -1;

    for(int i = 0; i < nfds; i++)
    {
        if(fds[i] != -1)
        {
            FD_SET(fds[i], readfds);
            if(fds[i] > max_fd)
            {
                max_fd = fds[i];
            }
        }
    }

    if(max_fd == -1)
    {
        return -1;
    }

    timeout.tv_sec  = 60;
    timeout.tv_usec = 0;
    result          = select(max_fd + 1, readfds, NULL, NULL, &timeout);

    if(result == -1)
    {
        perror("select");
    }

    return result;
}

static void process_fds(int *fds, int nfds, fd_set *readfds, bool *done)
{
    bool all_closed;

    all_closed = true;

    for(int i = 0; i < nfds; i++)
    {
        if(fds[i] != -1 && FD_ISSET(fds[i], readfds))
        {
            int result;

            result = handle_fd_read(fds[i], readfds);

            if(result != 0)
            {
                fds[i] = -1;
            }
        }

        if(fds[i] != -1)
        {
            all_closed = false;
        }
    }

    if(all_closed)
    {
        printf("All monitored sources have reached EOF or been closed.\n");
        *done = true;
    }
}

static int handle_fd_read(int fd, fd_set *readfds)
{
    char    buffer[128];
    ssize_t bytes;
    int     result;

    bytes = read(fd, buffer, sizeof(buffer) - 1);

    if(bytes > 0)
    {
        buffer[bytes] = '\0';
        printf("Input from fd %d: %s\n", fd, buffer);
        result = 0;
    }
    else if(bytes == 0)
    {
        printf("EOF reached on fd %d. Clearing file descriptor.\n", fd);
        FD_CLR(fd, readfds);
        close(fd);
        result = 1;
    }
    else
    {
        perror("read");
        FD_CLR(fd, readfds);
        close(fd);
        result = -1;
    }

    return result;
}
