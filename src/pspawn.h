#pragma once

#include <sys/types.h>

struct process {
    pid_t pid;
    int in;
    int out;
};

struct process pspawn(char *const *argv);
