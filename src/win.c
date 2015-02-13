#include "win.h"

#include <stdlib.h>

window *window_append(window *win, window *item)
{
    if (win == NULL)
    {
        return item;
    }

    window *ptr;
    for (ptr = win; ptr->next != NULL; ptr = ptr->next) {}
    ptr->next = item;
    return win;
}

void window_free(window *win)
{
    while (win != NULL)
    {
        window *tmp = win;
        win = win->next;
        free(tmp);
    }
}
