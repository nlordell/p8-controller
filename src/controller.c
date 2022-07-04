#include <errno.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <SDL.h>

struct serialdevice {
    FILE *clock;
    FILE *data;
};

static int device_init(void);
static void device_quit(void);

static int device_connect(struct serialdevice *device);
static void device_close(struct serialdevice *device);

#define NCONTROLLERS 8
struct controllers {
    atomic_flag updated;
    SDL_GameController *handles[NCONTROLLERS];
};

static struct controllers *controllers_init(void);
static int controllers_parseindex(char const *b, size_t len);
static void controllers_queueupdate(struct controllers *controllers);
static int controllers_write(struct controllers *controllers, int index, FILE *data);

int handler_thread(void *data) {
    struct serialdevice device = {0};
    struct controllers *controllers = (struct controllers *)data;

    SDL_Event quit = {0};
    quit.type = SDL_QUIT;

    ssize_t len;
    char *buffer = NULL;
    size_t n;

    while (true) {
        SDL_Log("connecting serial device...");
        if (device_connect(&device) != 0) {
            SDL_PushEvent(&quit);
            break;
        }
        SDL_Log("connected serial device");

        while ((len = getline(&buffer, &n, device.clock)) > 0) {
            int index = controllers_parseindex(buffer, len);
            if (index < 0) {
                fwrite(buffer, sizeof(char), len, stdout);
                fflush(stdout);
                continue;
            }

            SDL_LogVerbose(SDL_LOG_CATEGORY_APPLICATION, "query controller %d", index);
            if (controllers_write(controllers, index, device.data) != 0) {
                break;
            }
        }

        device_close(&device);
        SDL_Log("disconnected serial device");
    }

    return 0;
}

int main(int argc, char **argv) {
    if (device_init() != 0) {
        exit(EXIT_FAILURE);
    }
    atexit(device_quit);

    if (SDL_Init(SDL_INIT_GAMECONTROLLER) != 0) {
        fprintf(stderr, "ERROR: SDL initialization: %s\n", SDL_GetError());
        exit(EXIT_FAILURE);
    }
    atexit(SDL_Quit);

    struct controllers *controllers = controllers_init();

    SDL_Thread *handler = SDL_CreateThread(handler_thread, "handler", controllers);
    if (handler == NULL) {
        fprintf(stderr, "ERROR: handler thread creation: %s\n", SDL_GetError());
        exit(EXIT_FAILURE);
    }
    SDL_DetachThread(handler);

    SDL_Event event;
    while (SDL_WaitEvent(&event)) {
        switch (event.type) {
        case SDL_QUIT:
            goto quit;
        case SDL_CONTROLLERDEVICEADDED:
        case SDL_CONTROLLERDEVICEREMOVED:
            controllers_queueupdate(controllers);
            break;
        }
    }

quit:
    SDL_Log("quitting...");

    return EXIT_SUCCESS;
}

static char clockpath[] = "controller.clock";
static char datapath[] = "controller.data";

int device_init(void) {
    if (mkfifo(clockpath, S_IRUSR | S_IWUSR) != 0) {
        perror("ERROR: failed to create clock FIFO");
        return -1;
    }
    if (mkfifo(datapath, S_IRUSR | S_IWUSR) != 0) {
        perror("ERROR: failed to create data FIFO");
        if (remove(clockpath) != 0) {
            perror("ERROR: failed to remove clock FIFO");
        }
        return -1;
    }

    printf("Device initialized. Start PICO-8 with:\n    pico8 -o %s -i %s\n", clockpath, datapath);

    return 0;
}

void device_quit(void) {
    if (remove(clockpath) != 0) {
        perror("ERROR: failed to remove clock FIFO");
    }
    if (remove(datapath) != 0) {
        perror("ERROR: failed to remove data FIFO");
    }
}

int device_connect(struct serialdevice *device) {
    FILE *clock = fopen(clockpath, "r");
    if (clock == NULL) {
        perror("ERROR: failed to open clock FIFO");
        return -1;
    }

    FILE *data = fopen(datapath, "w");
    if (data == NULL) {
        perror("ERROR: failed to open clock FIFO");
        if (fclose(clock) != 0) {
            perror("ERROR: failed to close clock FIFO");
        }
        return -1;
    }

    device->clock = clock;
    device->data = data;
    return 0;
}

void device_close(struct serialdevice *device) {
    if (device->clock != NULL && fclose(device->clock) != 0) {
        perror("ERROR: failed to close clock FIFO");
    }
    if (device->data != NULL && fclose(device->data) != 0) {
        perror("ERROR: failed to close data FIFO");
    }
    *device = (struct serialdevice){0};
}

struct controllers *controllers_init(void) {
    struct controllers *controllers = malloc(sizeof(struct controllers));

    controllers->updated = (atomic_flag)ATOMIC_FLAG_INIT;
    for (int i = 0; i < NCONTROLLERS; ++i) {
        controllers->handles[i] = NULL;
    }

    return controllers;
}

int controllers_parseindex(char const *s, size_t len) {
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

int controllers_write(struct controllers *controllers, int index, FILE *data) {
    if (atomic_flag_test_and_set(&controllers->updated) == false) {
        for (int i = 0; i < NCONTROLLERS; ++i) {
            SDL_GameControllerClose(controllers->handles[i]);
            controllers->handles[i] = NULL;
        }

        SDL_GameController *handle;
        for (int i = 0; i < SDL_NumJoysticks(); ++i) {
            if (!SDL_IsGameController(i)) {
                continue;
            }

            handle = SDL_GameControllerOpen(i);
            if (handle == NULL) {
                fprintf(stderr, "ERROR: failed opening controller %d: %s\n", i, SDL_GetError());
                continue;
            }

            controllers->handles[i] = handle;
        }
    }

    SDL_GameController *handle = index >= 0 && index < NCONTROLLERS
        ? controllers->handles[index]
        : NULL;

    struct {
        Sint16 axis[NAXES];
        Uint8 button[NBUTTONS];
    } state;

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

    if (fwrite(&state, sizeof(state), 1, data) != 1) {
        perror("ERROR: failed writing to data line");
        return -1;
    }
    if (fflush(data) != 0) {
        perror("ERROR: failed to flush data line");
        return -1;
    }

    return 0;
}

void controllers_queueupdate(struct controllers *controllers) {
    atomic_flag_clear(&controllers->updated);
}
