#ifndef I3_EASYFOCUS_WIN
#define I3_EASYFOCUS_WIN

#include <stdint.h>

typedef struct window
{
    struct window *next;
    unsigned long id;
    uint32_t win_id;
    struct
    {
        int x;
        int y;
    } position;
} Window;

Window *window_append(Window *win, Window *item);
void window_free(Window *win);

#endif
