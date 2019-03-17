#ifndef I3_EASYFOCUS_XCB
#define I3_EASYFOCUS_XCB

#include <xcb/xcb.h>
#include "win_type.h"

int xcb_init();
char *xcb_keysym_to_string(xcb_keysym_t keysym);
int xcb_register_configure_notify();
uint16_t xcb_modifier_string_to_mask(char *modifier);
int xcb_grab_keysym(xcb_keysym_t keysym, uint16_t mod_mask);
xcb_keysym_t xcb_wait_for_user_input();
int xcb_create_text_window(int pos_x, int pos_y, WindowType windowType, const char* label);
void xcb_finish();

#endif
