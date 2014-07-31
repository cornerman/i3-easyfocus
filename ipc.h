#ifndef I3_EASYFOCUS_IPC
#define I3_EASYFOCUS_IPC

#include <i3ipc-glib/i3ipc-glib.h>

int ipc_init();
i3ipcConnection *ipc_connection();
GList *ipc_visible_windows();
int ipc_focus_window(int win_id);
void ipc_finish();

#endif
