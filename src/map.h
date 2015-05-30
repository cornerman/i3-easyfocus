#ifndef I3_EASYFOCUS_MAP
#define I3_EASYFOCUS_MAP

#include <xcb/xcb.h>
#include "win.h"

void map_init();
xcb_keysym_t map_add(Window *win);
Window *map_get(xcb_keysym_t keysym);

#endif
