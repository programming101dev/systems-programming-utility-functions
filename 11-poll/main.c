#include <fcntl.h>
#include <poll.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static int  handle_fd_read(int fd);
static int  wait_for_input(struct pollfd *fds, nfds_t nfds);
static void process_fds(struct pollfd *fds, nfds_t nfds, bool *done);
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
    struct pollfd fds[3];
    bool          done = false;
    nfds_t        nfds = 3;

    // Initialize pollfd array
    fds[0].fd     = STDIN_FILENO;
    fds[0].events = POLLIN;
    fds[1].fd     = file_fd;
    fds[1].events = POLLIN;
    fds[2].fd     = fifo_fd;
    fds[2].events = POLLIN;

    while(!done)
    {
        int ready = wait_for_input(fds, nfds);

        if(ready == -1)
        {
            *ret = 1;
            done = true;
        }
        else if(ready == 0)
        {
            printf("Timeout: No data within 60 seconds.\n");
            done = true;
        }
        else
        {
            process_fds(fds, nfds, &done);
        }
    }
}

static int wait_for_input(struct pollfd *fds, nfds_t nfds)
{
    int timeout = 60000;    // Timeout of 60 seconds
    int result  = poll(fds, nfds, timeout);

    if(result == -1)
    {
        perror("poll");
    }

    return result;
}

static void process_fds(struct pollfd *fds, nfds_t nfds, bool *done)
{
    bool all_closed = true;

    for(nfds_t i = 0; i < nfds; i++)
    {
        if(fds[i].fd != -1 && ((unsigned int)fds[i].revents & POLLIN) != 0)
        {
            int result = handle_fd_read(fds[i].fd);

            if(result != 0)
            {
                fds[i].fd = -1;    // Mark fd as closed
            }
        }

        if(fds[i].fd != -1)
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

static int handle_fd_read(int fd)
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
        close(fd);
        result = 1;
    }
    else
    {
        perror("read");
        close(fd);
        result = -1;
    }

    return result;
}
