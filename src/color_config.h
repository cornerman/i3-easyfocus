#ifndef I3_EASYFOCUS_COLOR_CONFIG
#define I3_EASYFOCUS_COLOR_CONFIG

#include <stdint.h>
#include "rgb.h"

typedef struct color_config
{
    Rgb urgent_bg;
    Rgb urgent_fg;
    Rgb focused_bg;
    Rgb focused_fg;
    Rgb unfocused_bg;
    Rgb unfocused_fg;
} ColorConfig;

#endif
