#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <errno.h>
#include <signal.h>
#include <spawn.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <pthread.h>

#include "pspawn.h"
#include "try.h"

static void *process_handler(void *p);

static void *counter_thread(void *data);
static ssize_t write_count(int pipe);

int main(int argc, char **argv) {
    char *const pico8_args[] = {"pico8", "cart/controller.p8", NULL};
    struct process pico8 = pspawn(pico8_args);

    pthread_t count;
    TRY(pthread_create(&count, NULL, counter_thread, NULL));

    pthread_t handler;
    TRY(pthread_create(&handler, NULL, process_handler, (void *)&pico8));

    if (waitpid(pico8.pid, NULL, 0) != pico8.pid) {
        TRY(errno);
    }

    return EXIT_SUCCESS;
}

void *process_handler(void *data) {
    struct process p = *(struct process *)data;
    char buffer[1024];

    write_count(p.in);
    while (
        read(p.out, buffer, sizeof(buffer)) > 0 &&
        write_count(p.in) > 0
    ) {}

    TRY(errno);
    return NULL;
}

static atomic_uint counter;

void *counter_thread(void *data) {
    struct timespec duration = {
        .tv_sec = 0,
        .tv_nsec = 62500000,
    };

    while (true) {
        nanosleep(&duration, NULL);
        atomic_fetch_add(&counter, 0x1000);
    }

    return NULL;
}

ssize_t write_count(int pipe) {
    union {
        uint32_t count;
        char bytes[5];
    } buffer;
    buffer.count = (uint32_t)atomic_load(&counter);

    // Make sure to write a newline, since it seems like PICO-8 will buffer
    // stdin reads until it gets one.
    buffer.bytes[4] = '\n';

    char *ptr = buffer.bytes;
    size_t rem = sizeof(buffer.bytes);
    while (rem > 0) {
        ssize_t n = write(pipe, ptr, rem);
        ptr += n;
        rem -= n;
    }

    return sizeof(buffer.bytes);
}
