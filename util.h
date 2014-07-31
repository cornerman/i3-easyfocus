#ifndef I3_EASYFOCUS_UTIL
#define I3_EASYFOCUS_UTIL

#define LOG(fmt, ...)                                                  \
    do {                                                               \
        if (DEBUG) {                                                   \
            printf("[%s:%d] " fmt, __FILE__, __LINE__, ##__VA_ARGS__); \
        }                                                              \
    } while (0)

#endif
