#ifndef I3_EASYFOCUS_XCB
#define I3_EASYFOCUS_XCB

int xcb_init();
int xcb_main_window(char *keysym_out);
int xcb_child_window(int pos_x, int pos_y, char *desc);
void xcb_finish();

#endif
