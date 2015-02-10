#ifndef I3_EASYFOCUS_XCB
#define I3_EASYFOCUS_XCB

int xcb_init();
int xcb_register_for_key_event(char key);
int xcb_wait_for_key_event(char *key);
int xcb_create_text_window(int pos_x, int pos_y, char *desc);
void xcb_finish();

#endif
