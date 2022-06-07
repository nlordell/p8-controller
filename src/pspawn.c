#include <signal.h>
#include <spawn.h>
#include <unistd.h>

#include "pspawn.h"
#include "try.h"

extern char **environ;

struct process pspawn(char *const *argv) {
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
