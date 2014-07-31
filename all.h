#ifndef I3_EASYFOCUS_ALL
#define I3_EASYFOCUS_ALL

#include <stdio.h>
#include <stdlib.h>

#define DEBUG 0

#if defined(LOG)
#undef LOG
#endif
#define LOG(fmt, ...)                                                  \
    do {                                                               \
        if (DEBUG) {                                                   \
            printf("[%s:%d] " fmt, __FILE__, __LINE__, ##__VA_ARGS__); \
        }                                                              \
    } while (0)

#endif
