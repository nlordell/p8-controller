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

#include <SDL.h>

#define PANIC(x) \
    do { \
        fprintf(stderr, "ERROR: %s (%s L%d)\n", (x), __FILE__, __LINE__); \
        exit(EXIT_FAILURE); \
    } while (false)
#define TRY(x) \
    do { \
        int result; \
        if ((result = (x)) != 0) { \
            result = result == -1 ? errno : result; \
            PANIC(strerror(result)); \
        } \
    } while (false)

struct process {
    pid_t pid;
    int in;
    int out;
};

static struct process process_spawn(char * const *argv);

struct reader {
    int fd;
    size_t len;
    size_t pos;
    char buffer[1024];
};

static struct reader reader_init(int fd);
static size_t reader_readline(struct reader *r, char *buffer, size_t buflen, bool *truncated);

struct controller {
    SDL_GameController *handle;
};

static void controller_writestate(struct controller *c, int fd);

static char const marker[] = u8"☉☉\n";

int main(int argc, char **argv) {
    if (SDL_Init(SDL_INIT_GAMECONTROLLER) < 0) {
        PANIC(SDL_GetError());
    }

    char * const pico8_args[] = {"pico8", "cart/controller.p8", NULL};
    struct process pico8 = process_spawn(pico8_args);

    struct reader reader = reader_init(pico8.out);
    struct controller controller = {0};

    char buffer[1024];
    size_t len;
    bool truncated;
    while ((len = reader_readline(&reader, buffer, sizeof(buffer), &truncated)) > 0) {
        if (len == sizeof(marker)-1 && memcmp(buffer, marker, len) == 0) {
            controller_writestate(&controller, pico8.in);
        } else {
            fwrite(buffer, sizeof(char), len, stdout);
            TRY(ferror(stdout));
            while (truncated) {
                if ((len = reader_readline(&reader, buffer, sizeof(buffer), &truncated)) == 0) {
                    goto eof;
                }
                fwrite(buffer, sizeof(char), len, stdout);
                TRY(ferror(stdout));
            }
        }
    }
eof:

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

struct reader reader_init(int fd) {
    return (struct reader) {
        .fd = fd,
        .pos = 0,
        .len = 0,
    };
}

size_t reader_readline(struct reader *r, char *buffer, size_t buflen, bool *truncated) {
    int i = 0;
    bool tr = true;
    while (i < buflen) {
        if (r->pos == r->len) {
            r->pos = 0;
            r->len = read(r->fd, r->buffer, sizeof(r->buffer));
            TRY(errno);
        }

        if (r->len == 0) {
            break;
        }

        char next = r->buffer[r->pos++];
        buffer[i++] = next;

        if (next == '\n') {
            tr = false;
            break;
        }
    }

    if (truncated != NULL) {
        *truncated = tr;
    }
    return i;
}

static bool controller_open(struct controller *c) {
    if (!SDL_GameControllerGetAttached(c->handle)) {
        if (c->handle != NULL) {
            SDL_GameControllerClose(c->handle);
            c->handle = NULL;
        }

        int joysticks = SDL_NumJoysticks();
        for (int i = 0; i < joysticks && c->handle == NULL; ++i) {
            if (SDL_IsGameController(i)) {
                c->handle = SDL_GameControllerOpen(i);
            }
        }

        if (c->handle == NULL) {
            return false;
        }
    }

    SDL_GameControllerUpdate();
    return true;
}

void controller_writestate(struct controller *c, int fd) {
    union {
        struct {
            int16_t axis[SDL_CONTROLLER_AXIS_MAX];
            uint8_t button[SDL_CONTROLLER_BUTTON_MAX];
            char newline;
        } state;
        char bytes[34];
    } buffer;
    _Static_assert(sizeof(buffer.bytes) == sizeof(buffer.state), "too many buttons");

    if (controller_open(c)) {
        for (int i = 0; i < SDL_CONTROLLER_AXIS_MAX; ++i) {
            buffer.state.axis[i] = SDL_GameControllerGetAxis(c->handle, i);
        }
        for (int i = 0; i < SDL_CONTROLLER_BUTTON_MAX; ++i) {
            buffer.state.button[i] = SDL_GameControllerGetButton(c->handle, i) * 0xff;
        }
    } else {
        memset(buffer.bytes, 0, sizeof(buffer.bytes) - 1);
    }
    buffer.state.newline = '\n';

    char *ptr = buffer.bytes;
    size_t rem = sizeof(buffer.bytes);
    while (rem > 0) {
        ssize_t n = write(fd, ptr, rem);
        TRY(errno);

        ptr += n;
        rem -= n;
    }
}
