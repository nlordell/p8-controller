#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <spawn.h>
#include <stdbool.h>
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
    FILE *in;
    FILE *out;
};

static struct process process_spawn(char * const *argv);
static void process_wait(struct process *p);

#define NCONTROLLERS 8
struct controllers {
    SDL_GameController *handles[NCONTROLLERS];
};

static int controller_parseindex(char const *b, size_t len);
static void controller_writestate(struct controllers *c, int device_index, FILE *f);
static void controller_close(struct controllers *c);

int main(int argc, char *argv[]) {
    char *pico8_args[argc + 1];
    pico8_args[0] = "pico8";
    for (int i = 1; i < argc; ++i) {
        pico8_args[i] = argv[i];
    }
    pico8_args[argc] = NULL;

    if (SDL_Init(SDL_INIT_GAMECONTROLLER) < 0) {
        PANIC(SDL_GetError());
    }
    atexit(SDL_Quit);

    struct process pico8 = process_spawn(pico8_args);
    struct controllers controllers = {0};

    ssize_t len;
    char *buffer = NULL;
    size_t n;
    while ((len = getline(&buffer, &n, pico8.out)) > 0) {
        int device_index = controller_parseindex(buffer, len);
        if (device_index >= 0) {
            controller_writestate(&controllers, device_index, pico8.in);
        } else {
            fwrite(buffer, sizeof(char), len, stdout);
            TRY(ferror(stdout));
        }
    }

    process_wait(&pico8);
    controller_close(&controllers);
    free(buffer);

    return EXIT_SUCCESS;
}

extern char **environ;

struct process process_spawn(char * const *argv) {
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

    FILE *fin = fdopen(in[W], "w");
    TRY(fin == NULL);
    FILE *fout = fdopen(out[R], "r");
    TRY(fout == NULL);

    close(in[R]);
    close(out[W]);
    posix_spawn_file_actions_destroy(&file_actions);
    posix_spawnattr_destroy(&attr);

    return (struct process){pid, fin, fout};
}

void process_wait(struct process *p) {
    if (waitpid(p->pid, NULL, 0) != p->pid) {
        TRY(errno);
    }
    TRY(fclose(p->in));
    TRY(fclose(p->out));

    *p = (struct process){0};
}

int controller_parseindex(char const *s, size_t len) {
    // parse controller request of the form: "☉$INDEX☉\n"

    char marker[] = u8"☉0☉\n";
    if (len != sizeof(marker) - 1) {
        return -1;
    }

    size_t i = sizeof(u8"☉") - 1;
    marker[i] = s[i];
    if (memcmp(s, marker, len) != 0) {
        return -1;
    }

    char index = marker[i];
    if (index < '0' || index >= '0' + NCONTROLLERS) {
        return -1;
    }

    return index - '0';
}

#define NAXES 6
static const SDL_GameControllerAxis axes[NAXES] = {
    SDL_CONTROLLER_AXIS_LEFTX,
    SDL_CONTROLLER_AXIS_LEFTY,
    SDL_CONTROLLER_AXIS_RIGHTX,
    SDL_CONTROLLER_AXIS_RIGHTY,
    SDL_CONTROLLER_AXIS_TRIGGERLEFT,
    SDL_CONTROLLER_AXIS_TRIGGERRIGHT,
};

#define NBUTTONS 18
static const SDL_GameControllerButton buttons[NBUTTONS] = {
    SDL_CONTROLLER_BUTTON_A,
    SDL_CONTROLLER_BUTTON_B,
    SDL_CONTROLLER_BUTTON_X,
    SDL_CONTROLLER_BUTTON_Y,
    SDL_CONTROLLER_BUTTON_LEFTSHOULDER,
    SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,
    SDL_CONTROLLER_BUTTON_INVALID,
    SDL_CONTROLLER_BUTTON_INVALID,
    SDL_CONTROLLER_BUTTON_BACK,
    SDL_CONTROLLER_BUTTON_START,
    SDL_CONTROLLER_BUTTON_LEFTSTICK,
    SDL_CONTROLLER_BUTTON_RIGHTSTICK,
    SDL_CONTROLLER_BUTTON_DPAD_UP,
    SDL_CONTROLLER_BUTTON_DPAD_DOWN,
    SDL_CONTROLLER_BUTTON_DPAD_LEFT,
    SDL_CONTROLLER_BUTTON_DPAD_RIGHT,
    SDL_CONTROLLER_BUTTON_GUIDE,
    SDL_CONTROLLER_BUTTON_INVALID,
};

#define NAXISBUTTONS 2
#define AXISBUTTON_THRESHOLD 30000
static const struct {
    int index;
    SDL_GameControllerAxis axis;
} axisbuttons[NAXISBUTTONS] = {
    {6, SDL_CONTROLLER_AXIS_TRIGGERLEFT},
    {7, SDL_CONTROLLER_AXIS_TRIGGERRIGHT},
};

static SDL_GameController *controller_open(struct controllers *c, int i) {
    assert(i >= 0 && i < NCONTROLLERS);

    SDL_GameControllerUpdate();
    if (SDL_IsGameController(i) == SDL_FALSE) {
        return NULL;
    }

    // SDL uses reference-counted controller objects. Abuse this and always open
    // a new handle, then free the old one. This ensures our controllers array
    // always has up to date handles without having to deal with SDL events
    // which would require a separate thread.
    SDL_GameController *handle = SDL_GameControllerOpen(i);
    if (handle == NULL) {
        PANIC(SDL_GetError());
    }

    SDL_GameControllerClose(c->handles[i]);
    c->handles[i] = handle;

    return handle;
}

void controller_writestate(struct controllers *c, int device_index, FILE *f) {
    struct {
        Sint16 axis[NAXES];
        Uint8 button[NBUTTONS];
    } state;

    SDL_GameController *handle = controller_open(c, device_index);
    if (handle != NULL) {
        for (int i = 0; i < NAXES; ++i) {
            state.axis[i] = SDL_GameControllerGetAxis(handle, axes[i]);
        }
        for (int i = 0; i < NBUTTONS; ++i) {
            state.button[i] = buttons[i] != SDL_CONTROLLER_BUTTON_INVALID
                ? SDL_GameControllerGetButton(handle, buttons[i]) * 0xff
                : 0;
        }
        for (int i = 0; i < NAXISBUTTONS; ++i) {
            bool pressed = SDL_GameControllerGetAxis(handle, axisbuttons[i].axis) > AXISBUTTON_THRESHOLD;
            state.button[axisbuttons[i].index] = pressed * 0xff;
        }
    } else {
        memset(&state, 0, sizeof(state));
    }

    fwrite(&state, sizeof(state), 1, f);
    fflush(f);

    TRY(ferror(f));
}

void controller_close(struct controllers *c) {
    for (int i = 0; i < NCONTROLLERS; ++i) {
        SDL_GameControllerClose(c->handles[i]);
    }
}
