#ifndef I3_EASYFOCUS_IPC
#define I3_EASYFOCUS_IPC

#include <i3ipc-glib/i3ipc-glib.h>

int ipc_init();
i3ipcConnection *ipc_connection();
void ipc_finish();

#endif
