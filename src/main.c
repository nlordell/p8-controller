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

#define TRY(x) \
    do { \
        int result; \
        if ((result = (x)) != 0) { \
            result = result == -1 ? errno : result; \
            fprintf(stderr, "ERROR: %s (%s L%d)\n", strerror(result), __FILE__, __LINE__); \
            exit(EXIT_FAILURE); \
        } \
    } while (false)

struct process {
    pid_t pid;
    int in;
    int out;
};

static struct process process_spawn(char *const *argv);

static void *counter_thread(void *data);
static ssize_t write_count(int pipe);

int main(int argc, char **argv) {
    char *const pico8_args[] = {"pico8", "cart/controller.p8", NULL};
    struct process pico8 = process_spawn(pico8_args);

    pthread_t count;
    TRY(pthread_create(&count, NULL, counter_thread, NULL));

    char buffer[1024];
    while (
        read(pico8.out, buffer, sizeof(buffer)) > 0 &&
        write_count(pico8.in) > 0
    ) {
        // TODO(nlordell): Instead of just assuming anything on stdout is a
        // request to sample a gamepad, we should parse out some "control
        // characters" from the process's stdout and forward the rest of the
        // data to our stdout (this allows things like `printh` to continue
        // working in PICO-8.
    }
    TRY(errno);

    if (waitpid(pico8.pid, NULL, 0) != pico8.pid) {
        TRY(errno);
    }

    return EXIT_SUCCESS;
}

extern char **environ;

struct process process_spawn(char *const *argv) {
    const int R = 0;
    const int W = 1;

    int in[2], out[2];
    TRY(pipe(in));
    TRY(pipe(out));

    posix_spawn_file_actions_t file_actions;
    TRY(posix_spawn_file_actions_init(&file_actions));
    TRY(posix_spawn_file_actions_adddup2(&file_actions, in[R], STDIN_FILENO));
    TRY(posix_spawn_file_actions_adddup2(&file_actions, out[W], STDOUT_FILENO));
    for (int i = 0; i < 2; ++i) {
        TRY(posix_spawn_file_actions_addclose(&file_actions, in[i]));
        TRY(posix_spawn_file_actions_addclose(&file_actions, out[i]));
    }

    sigset_t mask;
    TRY(sigemptyset(&mask));

    sigset_t def;
    TRY(sigemptyset(&def));
    TRY(sigaddset(&def, SIGPIPE));

    posix_spawnattr_t attr;
    TRY(posix_spawnattr_init(&attr));
    TRY(posix_spawnattr_setsigmask(&attr, &mask));
    TRY(posix_spawnattr_setsigdefault(&attr, &def));
    TRY(posix_spawnattr_setflags(&attr, POSIX_SPAWN_SETSIGDEF | POSIX_SPAWN_SETSIGMASK));

    pid_t pid;
    TRY(posix_spawnp(&pid, argv[0], &file_actions, &attr, argv, environ));

    close(in[R]);
    close(out[W]);
    posix_spawn_file_actions_destroy(&file_actions);
    posix_spawnattr_destroy(&attr);

    return (struct process){
        .pid = pid,
        .in = in[W],
        .out = out[R],
    };
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
