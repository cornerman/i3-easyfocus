#include "util.h"
#include "config.h"
#include "map.h"

#include <X11/keysym.h>
#include <X11/keysymdef.h>
#include <stdlib.h>

#define LENGTH_AVY (sizeof(label_avy_keysyms) / sizeof(label_avy_keysyms[0]))
static xcb_keysym_t label_avy_keysyms[] = LABEL_KEYSYMS_AVY;

#define LENGTH_COLEMAK (sizeof(label_colemak_keysyms) / sizeof(label_colemak_keysyms[0]))
static xcb_keysym_t label_colemak_keysyms[] = LABEL_KEYSYMS_COLEMAK;

#define LENGTH_ALPHA (sizeof(label_alpha_keysyms) / sizeof(label_alpha_keysyms[0]))
static xcb_keysym_t label_alpha_keysyms[] = LABEL_KEYSYMS_ALPHA;

static xcb_keysym_t *label_keysyms = NULL;

static Window **win_map = NULL;
static size_t map_length = 0;
static size_t current = 0;

void map_init(label_key_mode_e mode)
{
    current = 0;

    switch(mode)
    {
    case LABEL_KEY_MODE_AVY:
        label_keysyms = label_avy_keysyms;
        map_length = LENGTH_AVY;
        break;
    case LABEL_KEY_MODE_ALPHA:
        label_keysyms = label_alpha_keysyms;
        map_length = LENGTH_ALPHA;
        break;
    case LABEL_KEY_MODE_COLEMAK:
        label_keysyms = label_colemak_keysyms;
        map_length = LENGTH_COLEMAK;
        break;
    }
    win_map = calloc(map_length, sizeof(Window*));
}

xcb_keysym_t map_add(Window *win)
{
    if (current >= map_length)
    {
        LOG("too many windows, only configured %lu keysyms\n", map_length);
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
    for (i = 0; i < map_length && i < current; i++)
    {
        if (label_keysyms[i] == keysym)
        {
            return win_map[i];
        }
    }

    LOG("selection not in range\n");
    return NULL;
}

void map_free()
{
    free(win_map);
    win_map = NULL;
    map_length = 0;
    current = 0;
}
