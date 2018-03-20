#ifndef I3_EASYFOCUS_IPC
#define I3_EASYFOCUS_IPC

#include "win.h"

typedef enum {
    CURRENT_OUTPUT,
    ALL_OUTPUTS,
    CURRENT_CONTAINER
} SearchArea;

typedef enum {
    BY_LOCATION,
    BY_NUMBER
} SortMethod;

int ipc_init();
Window *ipc_visible_windows(SearchArea search_area, SortMethod sort_method);
int ipc_focus_window(Window *window);
void ipc_finish();

#endif
