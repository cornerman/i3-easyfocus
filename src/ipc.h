#ifndef I3_EASYFOCUS_IPC
#define I3_EASYFOCUS_IPC

#include "win.h"

typedef enum {
    CURRENT_OUTPUT,
    ALL_OUTPUTS
} SearchArea;

int ipc_init();
Window *ipc_visible_windows(SearchArea search_area);
int ipc_focus_window(int id);
void ipc_finish();

#endif
