#ifndef I3_EASYFOCUS_UTIL
#define I3_EASYFOCUS_UTIL

#include <stdio.h>

#ifdef DEBUG
#define DEBUG_TEST 1
#else
#define DEBUG_TEST 0
#endif

#define LOG(fmt, ...)                                                  \
    do {                                                               \
        if (DEBUG_TEST) {                                              \
            printf("[%s:%d] " fmt, __FILE__, __LINE__, ##__VA_ARGS__); \
        }                                                              \
    } while (0)

#endif
