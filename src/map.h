#ifndef I3_EASYFOCUS_MAP
#define I3_EASYFOCUS_MAP

#include <xcb/xcb.h>

void map_init();
xcb_keysym_t map_add();
int map_get(xcb_keysym_t keysym);

#endif
