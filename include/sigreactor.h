#ifndef SIGREACTOR_H
#define SIGREACTOR_H

#include <signal.h>
#include <stddef.h>

#define SIGREACTOR_MAX_SIGNALS 64
#define SIGREACTOR_MAX_EVENTS 128

typedef enum {
    SR_EVENT_SIGNAL = 1,
    SR_EVENT_TIMER,
    SR_EVENT_CHILD
} SrEventType;

typedef struct {
    SrEventType type;
    int value;
} SrEvent;

typedef void (*SrSignalCallback)(int signal_number);
typedef void (*SrTimerCallback)(void);
typedef void (*SrChildCallback)(pid_t pid, int status);

int sr_init(void);

int sr_register_signal(
    int signal_number,
    SrSignalCallback callback
);

int sr_add_timer(
    int interval_ms,
    SrTimerCallback callback
);

int sr_watch_child(
    SrChildCallback callback
);

int sr_run(void);

void sr_stop(void);

void sr_cleanup(void);

const char *sr_signal_name(int signal_number);

size_t sr_dropped_events(void);

#endif
