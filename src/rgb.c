#include "rgb.h"

#include <stdlib.h>
#include "util.h"

int parse_rgb_string(const char *rgb_str, Rgb* rgb)
{
    if (rgb_str == NULL)
    {
        LOG("Cannot parse empty rgb string");
        return 1;
    }

    int r, g, b;
    int number_writes = sscanf(rgb_str, "%02x%02x%02x", &r, &g, &b);
    if (number_writes != 3 || r < 0 || g < 0 || b < 0)
    {
        LOG("Cannot parse hex rgb string: %s", rgb_str);
        return 1;
    }

    rgb->r = r * 257;
    rgb->g = g * 257;
    rgb->b = b * 257;
    return 0;
}
