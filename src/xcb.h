#ifndef I3_EASYFOCUS_XCB
#define I3_EASYFOCUS_XCB

#include <xcb/xcb.h>

int xcb_init();
char *xcb_keysym_to_string(xcb_keysym_t keysym);
int xcb_grab_keysym(xcb_keysym_t keysym);
int xcb_wait_for_key_event(xcb_keysym_t *keysym);
int xcb_create_text_window(int pos_x, int pos_y, const char* label);
void xcb_finish();

#endif
