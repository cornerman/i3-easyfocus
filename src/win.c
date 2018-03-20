#include "win.h"

#include <stdlib.h>

Window *window_append(Window *win, Window *item)
{
    if (win == NULL)
    {
        return item;
    }

    Window *ptr;
    for (ptr = win; ptr->next != NULL; ptr = ptr->next)
    {
    }
    ptr->next = item;
    return win;
}

void window_free(Window *win)
{
    while (win != NULL)
    {
        Window *tmp = win;
        win = win->next;
        free(tmp);
    }
}
