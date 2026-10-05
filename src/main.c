#define _GNU_SOURCE

#include "sigreactor.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

static int user_events = 0;

static int timer_ticks = 0;


static void on_usr1(int signo)
{
    (void)signo;

    user_events++;

    printf(
        "[EVENT] SIGUSR1 -> custom event count = %d\n",
        user_events
    );

    fflush(stdout);
}


static void on_usr2(int signo)
{
    (void)signo;

    printf(
        "[EVENT] SIGUSR2 -> diagnostic event\n"
    );

    fflush(stdout);
}


static void on_hup(int signo)
{
    (void)signo;

    printf(
        "[EVENT] SIGHUP -> configuration reload requested\n"
    );

    printf(
        "[ACTION] Configuration would be reloaded here.\n"
    );

    fflush(stdout);
}


static void on_int(int signo)
{
    (void)signo;

    printf(
        "[EVENT] SIGINT -> graceful shutdown requested\n"
    );

    fflush(stdout);
}


static void on_term(int signo)
{
    (void)signo;

    printf(
        "[EVENT] SIGTERM -> graceful shutdown requested\n"
    );

    fflush(stdout);
}


static void on_timer(void)
{
    timer_ticks++;

    printf(
        "[TIMER] periodic event #%d\n",
        timer_ticks
    );

    fflush(stdout);
}


static void on_child(
    pid_t pid,
    int status
)
{
    if (WIFEXITED(status))
    {
        printf(
            "[CHILD] PID %d exited with status %d\n",
            pid,
            WEXITSTATUS(status)
        );
    }
    else if (WIFSIGNALED(status))
    {
        printf(
            "[CHILD] PID %d terminated by signal %d\n",
            pid,
            WTERMSIG(status)
        );
    }

    fflush(stdout);
}


static void spawn_demo_child(void)
{
    fflush(NULL);

    pid_t pid = fork();

    if (pid == -1)
    {
        perror("fork");

        return;
    }

    if (pid == 0)
    {
        printf(
            "[CHILD] Worker PID %d started\n",
            getpid()
        );

        fflush(stdout);

        sleep(3);

        _exit(7);
    }

    printf(
        "[PARENT] Worker PID %d created\n",
        pid
    );

    fflush(stdout);
}


int main(void)
{
    printf(
        "==============================================\n"
    );

    printf(
        "      SigReactor - OS Event Framework\n"
    );

    printf(
        "==============================================\n"
    );

    printf(
        "PID: %d\n\n",
        getpid()
    );

    if (sr_init() != 0)
        return EXIT_FAILURE;


    if (sr_register_signal(
            SIGUSR1,
            on_usr1
        ) != 0)
    {
        return EXIT_FAILURE;
    }


    if (sr_register_signal(
            SIGUSR2,
            on_usr2
        ) != 0)
    {
        return EXIT_FAILURE;
    }


    if (sr_register_signal(
            SIGHUP,
            on_hup
        ) != 0)
    {
        return EXIT_FAILURE;
    }


    if (sr_register_signal(
            SIGINT,
            on_int
        ) != 0)
    {
        return EXIT_FAILURE;
    }


    if (sr_register_signal(
            SIGTERM,
            on_term
        ) != 0)
    {
        return EXIT_FAILURE;
    }


    if (sr_add_timer(
            5000,
            on_timer
        ) != 0)
    {
        fprintf(
            stderr,
            "Failed to create timer\n"
        );

        sr_cleanup();

        return EXIT_FAILURE;
    }


    if (sr_watch_child(
            on_child
        ) != 0)
    {
        fprintf(
            stderr,
            "Failed to enable child monitoring\n"
        );

        sr_cleanup();

        return EXIT_FAILURE;
    }


    printf(
        "Send SIGUSR1 : kill -USR1 %d\n",
        getpid()
    );

    printf(
        "Send SIGUSR2 : kill -USR2 %d\n",
        getpid()
    );

    printf(
        "Send SIGHUP  : kill -HUP  %d\n",
        getpid()
    );

    printf(
        "Send SIGTERM : kill -TERM %d\n",
        getpid()
    );

    printf(
        "Send SIGINT  : kill -INT  %d\n",
        getpid()
    );

    printf(
        "Timer        : every 5 seconds\n"
    );

    printf(
        "Child demo   : fork() + SIGCHLD monitoring\n\n"
    );

    fflush(stdout);


    spawn_demo_child();


    printf(
        "Waiting for events...\n\n"
    );

    fflush(stdout);


    sr_run();


    printf(
        "\nEvent loop stopped. Dropped events: %zu\n",
        sr_dropped_events()
    );


    sr_cleanup();

    return EXIT_SUCCESS;
}
