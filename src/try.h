#pragma once

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TRY(x) \
    do { \
        int result; \
        if ((result = (x)) != 0) { \
            result = result == -1 ? errno : result; \
            fprintf(stderr, "ERROR: %s (%s L%d)\n", strerror(result), __FILE__, __LINE__); \
            exit(EXIT_FAILURE); \
        } \
    } while (false)
