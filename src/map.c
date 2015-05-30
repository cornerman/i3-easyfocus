#include "util.h"
#include "config.h"
#include "map.h"

#include <X11/keysym.h>
#include <X11/keysymdef.h>

#define LENGTH (sizeof(label_keysyms) / sizeof(label_keysyms[0]))
static xcb_keysym_t label_keysyms[] = LABEL_KEYSYMS;

static Window *win_map[LENGTH];
static size_t current = 0;

void map_init()
{
    current = 0;
}

xcb_keysym_t map_add(Window* win)
{
    if (current >= LENGTH)
    {
        LOG("too many windows, only configured %lu keysyms\n", LENGTH);
        return XCB_NO_SYMBOL;
    }

    size_t key = current;
    current++;
    win_map[key] = win;
    return label_keysyms[key];
}

Window *map_get(xcb_keysym_t keysym)
{
    size_t i;
    for (i = 0; i < LENGTH; i++)
    {
        if (label_keysyms[i] == keysym)
        {
            return win_map[i];
        }
    }

    LOG("selection not in range\n");
    return NULL;
}
