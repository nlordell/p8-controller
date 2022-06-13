#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include <signal.h>
#include <unistd.h>

static void device_create();

int main(int argc, char **argv) {
    printf("hello!\n");

    device_create();

    while (true) {
        printf("...zZz...\n");
        sleep(1);
    }

    return EXIT_SUCCESS;
}

static char template[] = "/tmp/p8-controller.XXXXXX";
static char *dir = NULL;

static void device_cleanup(void) {
    printf("bye...\n");
}

static void device_sighandler(int signum) {
    device_cleanup();
    raise(signum);
}

static void device_setsighandler(int signum) {
    struct sigaction oldaction = {0};
    if (sigaction(signum, NULL, &oldaction) != 0) {
        perror("ERROR: requesting existing sigaction");
    }
    if (oldaction.sa_handler == SIG_IGN) {
        return;
    }

    struct sigaction action = {0};
    action.sa_handler = device_sighandler;
    action.sa_flags = SA_RESETHAND | SA_NODEFER;
    if (sigaction(signum, &action, NULL) != 0) {
        perror("ERROR: setting signal handler");
    }
}

static void device_create() {
    atexit(device_cleanup);
    device_setsighandler(SIGHUP);
    device_setsighandler(SIGINT);
    device_setsighandler(SIGQUIT);
    device_setsighandler(SIGTERM);
}
