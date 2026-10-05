#define _GNU_SOURCE

#include "sigreactor.h"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <sys/timerfd.h>
#include <sys/wait.h>
#include <unistd.h>

static int event_pipe[2] = {-1, -1};

static int timer_fd = -1;

static volatile sig_atomic_t running = 1;

static volatile sig_atomic_t watched_child = 0;

static size_t dropped = 0;

static SrSignalCallback
signal_callbacks[SIGREACTOR_MAX_SIGNALS];

static SrTimerCallback timer_callback = NULL;

static SrChildCallback child_callback = NULL;


static void async_signal_handler(int signo)
{
    int saved_errno = errno;

    ssize_t result = write(
        event_pipe[1],
        &signo,
        sizeof(signo)
    );

    if (result != (ssize_t)sizeof(signo))
    {
        /* Event could not be queued. */
    }

    errno = saved_errno;
}


static int set_nonblocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);

    if (flags == -1)
        return -1;

    return fcntl(
        fd,
        F_SETFL,
        flags | O_NONBLOCK
    );
}


static int register_with_sigaction(int signo)
{
    struct sigaction sa;

    memset(&sa, 0, sizeof(sa));

    sa.sa_handler = async_signal_handler;

    sigemptyset(&sa.sa_mask);

    sa.sa_flags = SA_RESTART;

    return sigaction(
        signo,
        &sa,
        NULL
    );
}


int sr_init(void)
{
    if (pipe(event_pipe) == -1)
    {
        perror("pipe");
        return -1;
    }

    if (set_nonblocking(event_pipe[0]) == -1 ||
        set_nonblocking(event_pipe[1]) == -1)
    {
        perror("fcntl");

        sr_cleanup();

        return -1;
    }

    memset(
        signal_callbacks,
        0,
        sizeof(signal_callbacks)
    );

    timer_callback = NULL;

    child_callback = NULL;

    timer_fd = -1;

    watched_child = 0;

    dropped = 0;

    running = 1;

    return 0;
}


int sr_register_signal(
    int signal_number,
    SrSignalCallback callback
)
{
    if (signal_number <= 0 ||
        signal_number >= SIGREACTOR_MAX_SIGNALS ||
        callback == NULL)
    {
        return -1;
    }

    if (register_with_sigaction(signal_number) == -1)
    {
        perror("sigaction");

        return -1;
    }

    signal_callbacks[signal_number] = callback;

    return 0;
}


int sr_add_timer(
    int interval_ms,
    SrTimerCallback callback
)
{
    if (interval_ms <= 0 ||
        callback == NULL)
    {
        return -1;
    }

    timer_fd = timerfd_create(
        CLOCK_MONOTONIC,
        TFD_CLOEXEC
    );

    if (timer_fd == -1)
    {
        perror("timerfd_create");

        return -1;
    }

    struct itimerspec timer;

    memset(&timer, 0, sizeof(timer));

    timer.it_value.tv_sec =
        interval_ms / 1000;

    timer.it_value.tv_nsec =
        (long)(interval_ms % 1000) * 1000000L;

    timer.it_interval = timer.it_value;

    if (timerfd_settime(
            timer_fd,
            0,
            &timer,
            NULL) == -1)
    {
        perror("timerfd_settime");

        close(timer_fd);

        timer_fd = -1;

        return -1;
    }

    timer_callback = callback;

    return 0;
}


int sr_watch_child(
    SrChildCallback callback
)
{
    if (callback == NULL)
        return -1;

    if (register_with_sigaction(SIGCHLD) == -1)
    {
        perror("sigaction(SIGCHLD)");

        return -1;
    }

    child_callback = callback;

    watched_child = 1;

    return 0;
}


static void process_signal(int signo)
{
    if (signo == SIGINT ||
        signo == SIGTERM)
    {
        running = 0;
    }

    if (signo > 0 &&
        signo < SIGREACTOR_MAX_SIGNALS &&
        signal_callbacks[signo] != NULL)
    {
        signal_callbacks[signo](signo);
    }
}


static void process_child_events(void)
{
    int status;

    pid_t pid;

    while ((pid = waitpid(
        -1,
        &status,
        WNOHANG
    )) > 0)
    {
        if (child_callback != NULL)
        {
            child_callback(
                pid,
                status
            );
        }
    }
}


int sr_run(void)
{
    struct pollfd fds[2];

    while (running)
    {
        int count = 0;

        int pipe_index = count;

        fds[count++] =
            (struct pollfd){
                .fd = event_pipe[0],
                .events = POLLIN
            };

        int timer_index = -1;

        if (timer_fd != -1)
        {
            timer_index = count;

            fds[count++] =
                (struct pollfd){
                    .fd = timer_fd,
                    .events = POLLIN
                };
        }

        int result = poll(
            fds,
            count,
            -1
        );

        if (result == -1)
        {
            if (errno == EINTR)
                continue;

            perror("poll");

            return -1;
        }

        if (fds[pipe_index].revents & POLLIN)
        {
            int signo;

            while (read(
                event_pipe[0],
                &signo,
                sizeof(signo)
            ) == (ssize_t)sizeof(signo))
            {
                process_signal(signo);
            }
        }

        if (timer_index != -1 &&
            (fds[timer_index].revents & POLLIN))
        {
            unsigned long long expirations;

            if (read(
                timer_fd,
                &expirations,
                sizeof(expirations)
            ) == (ssize_t)sizeof(expirations))
            {
                if (timer_callback != NULL)
                {
                    for (
                        unsigned long long i = 0;
                        i < expirations;
                        i++
                    )
                    {
                        timer_callback();
                    }
                }
            }
        }

        if (watched_child)
        {
            process_child_events();
        }
    }

    return 0;
}


void sr_stop(void)
{
    running = 0;
}


void sr_cleanup(void)
{
    if (timer_fd != -1)
        close(timer_fd);

    timer_fd = -1;

    if (event_pipe[0] != -1)
        close(event_pipe[0]);

    if (event_pipe[1] != -1)
        close(event_pipe[1]);

    event_pipe[0] = -1;

    event_pipe[1] = -1;
}


const char *sr_signal_name(
    int signal_number
)
{
    switch (signal_number)
    {
        case SIGINT:
            return "SIGINT";

        case SIGTERM:
            return "SIGTERM";

        case SIGHUP:
            return "SIGHUP";

        case SIGUSR1:
            return "SIGUSR1";

        case SIGUSR2:
            return "SIGUSR2";

        case SIGCHLD:
            return "SIGCHLD";

        default:
            return "UNKNOWN";
    }
}


size_t sr_dropped_events(void)
{
    return dropped;
}
