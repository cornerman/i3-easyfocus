#ifndef I3_EASYFOCUS_IPC
#define I3_EASYFOCUS_IPC

#include "win.h"

int ipc_init();
window *ipc_visible_windows();
int ipc_focus_window(int win_id);
void ipc_finish();

#endif
