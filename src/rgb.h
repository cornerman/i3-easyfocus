#ifndef I3_EASYFOCUS_RGB
#define I3_EASYFOCUS_RGB

#include <stdint.h>

typedef struct rgb
{
    uint16_t r;
    uint16_t g;
    uint16_t b;
} Rgb;

int parse_rgb_string(const char *rgb_str, Rgb* rgb);

#endif
