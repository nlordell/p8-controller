#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <signal.h>
#include <spawn.h>
#include <time.h>
#include <unistd.h>

#include <pthread.h>

struct process {
    pid_t pid;
    int stdin;
    int stdout;
};

static bool process_spawn(struct process *p, char *const *argv);
static void *process_handler(void *p);

static void *counter_thread(void *data);
static unsigned int current_count(void);
static ssize_t write_count(int pipe);

int main(int argc, char **argv) {
    struct process pico8;
    char *const pico8_args[] = {"pico8", "cart/controller.p8", NULL};
    if (process_spawn(&pico8, pico8_args)) {
        fprintf(stderr, "failed to spawn PICO-8 process\n");
        return EXIT_FAILURE;
    }

    pthread_t count;
    if (pthread_create(&count, NULL, counter_thread, NULL)) {
        fprintf(stderr, "failed to start counter\n");
        return EXIT_FAILURE;
    }

    pthread_t handler;
    if (pthread_create(&handler, NULL, process_handler, (void *)&pico8)) {
        fprintf(stderr, "failed to start process handler\n");
        return EXIT_FAILURE;
    }

    waitpid(pico8.pid, NULL, 0);

    return EXIT_SUCCESS;
}

extern char **environ;

bool process_spawn(struct process *p, char *const *argv) {
    const int R = 0;
    const int W = 1;

    bool success = false;

    int stdin[2], stdout[2];
    if (pipe(stdin) || pipe(stdout)) {
        fprintf(stderr, "unable to open pipe\n");
        goto err_pipes;
    }

    posix_spawn_file_actions_t file_actions;
    if (
        posix_spawn_file_actions_init(&file_actions) ||
        posix_spawn_file_actions_adddup2(&file_actions, stdin[R], STDIN_FILENO) ||
        posix_spawn_file_actions_adddup2(&file_actions, stdout[W], STDOUT_FILENO)
    ) {
        fprintf(stderr, "error configuring file actions\n");
        goto err_file_actions;
    }

    sigset_t mask;
    posix_spawnattr_t attr;
    if (
        sigemptyset(&mask) ||
        posix_spawnattr_init(&attr) ||
        posix_spawnattr_setsigmask(&attr, &mask) ||
        sigaddset(&mask, SIGPIPE) ||
        posix_spawnattr_setsigdefault(&attr, &mask) ||
        posix_spawnattr_setflags(&attr, POSIX_SPAWN_SETSIGDEF | POSIX_SPAWN_SETSIGMASK)
    ) {
        fprintf(stderr, "error configuring spawn attributes\n");
        goto err_attr;
    }

    success = posix_spawnp(&p->pid, argv[0], &file_actions, &attr, argv, environ) == 0;

    posix_spawnattr_destroy(&attr);

err_attr:
    posix_spawn_file_actions_destroy(&file_actions);

err_file_actions:
    if (success) {
        p->stdin = stdin[W];
        p->stdout = stdout[R];
    } else {
        for (int i = 0; i < 2; i++) {
            close(stdin[i]);
            close(stdout[i]);
        }
    }

err_pipes:
    return !success;
}

void *process_handler(void *data) {
    struct process *p = (struct process *)data;
    char buffer[1024];

    write_count(p->stdin);
    while (
        read(p->stdout, buffer, sizeof(buffer)) > 0 &&
        write_count(p->stdin) > 0
    ) {}

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

unsigned int current_count(void) {
    return atomic_load(&counter);
}

ssize_t write_count(int pipe) {
    uint32_t count = (uint32_t)current_count();

    char *buffer = (char *)&count;
    ssize_t rem = sizeof(count);
    while (rem > 0) {
        ssize_t n = write(pipe, buffer, rem);
        buffer += n;
        rem -= n;
    }

    // Make sure to write a newline, since it seems like PICO-8 will buffer
    // stdin reads until it gets one.
    write(pipe, "\n", 1);

    return sizeof(unsigned int) + 1;
}
