#ifndef I3_EASYFOCUS_WIN
#define I3_EASYFOCUS_WIN

#include <stdint.h>
#include "win_type.h"

typedef struct window
{
    struct window *next;
    unsigned long id;
    uint32_t win_id;
    WindowType type;
    struct
    {
        int x;
        int y;
    } position;
} Window;

Window *window_append(Window *win, Window *item);
void window_free(Window *win);

#endif
