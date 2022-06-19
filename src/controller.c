#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <SDL.h>

struct serialdevice {
    Uint32 event;
    FILE *clock;
    FILE *data;
};

static int device_create(struct serialdevice *device);
static void device_destroy(struct serialdevice *device);

int clock_thread(void *data) {
    struct serialdevice *device = (struct serialdevice *)data;

    FILE *clock = device->clock;

    SDL_Event event;
    SDL_zero(event);
    event.type = device->event;

    while ((event.user.code = fgetc(clock)) != EOF) {
        SDL_Log("got request!");
        if (SDL_PushEvent(&event) < 0) {
            SDL_Log("failed to push clock event: %s", SDL_GetError());
        }
    }

    SDL_Log("finished!");
    return 0;
}

int main(int argc, char **argv) {
    int exitcode = EXIT_SUCCESS;

    struct serialdevice device = {0};
    SDL_Thread *clock = NULL;
    SDL_GameController *controller = NULL;

    if (SDL_Init(SDL_INIT_GAMECONTROLLER) != 0) {
        fprintf(stderr, "ERROR: SDL initialization: %s\n", SDL_GetError());
        exitcode = EXIT_FAILURE;
        goto quit;
    }

    if (device_create(&device) != 0) {
        exitcode = EXIT_FAILURE;
        goto quit;
    }

    if ((clock = SDL_CreateThread(clock_thread, "clock", &device)) == NULL) {
        fprintf(stderr, "ERROR: clock creation: %s\n", SDL_GetError());
        exitcode = EXIT_FAILURE;
        goto quit;
    }

    SDL_Event event;
    while (SDL_WaitEvent(&event)) {
        if (event.type == SDL_QUIT) {
            break;
        }
        if (event.type == device.event) {
            SDL_Log("got event!");
        }
    }

quit:
    device_destroy(&device);

    SDL_WaitThread(clock, NULL);
    SDL_Quit();

    return exitcode;
}

static FILE *openfifo(char const *path, char const *mode) {
    if (mkfifo(path, S_IRUSR | S_IWUSR) != 0) {
        perror("failed to create FIFO");
        return NULL;
    }

    FILE *fifo = fopen(path, mode);
    if (fifo == NULL) {
        perror("failed to open FIFO");
        if (remove(path) != 0) {
            perror("failed to remove errored FIFO");
        }
        return NULL;
    }

    return fifo;
}

void closefifo(char const *path, FILE *fifo) {
    if (fifo == NULL) {
        return;
    }

    if (fclose(fifo) != 0) {
        perror("failed to close FIFO");
    }
    if (remove(path) != 0) {
        perror("failed to remove FIFO");
    }
}

static char clockpath[] = "controller.clock";
static char datapath[] = "controller.data";

int device_create(struct serialdevice *device) {
    device->clock = openfifo(clockpath, "r");
    if (device->clock == NULL) {
        device_destroy(device);
        return -1;
    }

    device->data = openfifo(datapath, "w");
    if (device->data == NULL) {
        device_destroy(device);
        return -1;
    }

    device->event = SDL_RegisterEvents(1);
    if (device->event == (Uint32)-1) {
        device_destroy(device);
        return -1;
    }

    return 0;
}

void device_destroy(struct serialdevice *device) {
    closefifo(clockpath, device->clock);
    closefifo(datapath, device->data);
    *device = (struct serialdevice){0};
}
