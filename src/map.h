#ifndef I3_EASYFOCUS_MAP
#define I3_EASYFOCUS_MAP

#include <xcb/xcb.h>
#include "win.h"

typedef enum
{
    LABEL_KEY_MODE_AVY,
    LABEL_KEY_MODE_ALPHA,
    LABEL_KEY_MODE_COLEMAK,

    LABEL_KEY_MODE_DEFAULT = LABEL_KEY_MODE_AVY,
} label_key_mode_e;

void map_init(label_key_mode_e mode);
xcb_keysym_t map_add(Window *win);
Window *map_get(xcb_keysym_t keysym);
void map_free();

#endif
