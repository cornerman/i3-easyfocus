#include "config.h"
#include "util.h"
#include "ipc.h"
#include "xcb.h"

#include <stdio.h>
#include <stdlib.h>
#include <glib.h>
#include <glib-object.h>

#define MIN_CHAR 97
#define MAX_CHAR 122
#define DIFF_CHAR MAX_CHAR - MIN_CHAR + 1

typedef struct char_map_t
{
    int map[DIFF_CHAR];
    char num;
} char_map;

static void create_window_label(i3ipcCon *con, char_map *curr)
{
    if (curr->num >= DIFF_CHAR)
    {
        LOG("too many windows, we only have %i characters\n", DIFF_CHAR);
        return;
    }

    i3ipcCon *parent = NULL;
    g_object_get(con, "parent", &parent, NULL);

    i3ipcRect *rect = NULL;
    g_object_get(con, "rect", &rect, NULL);

    i3ipcRect *parent_rect = NULL;
    g_object_get(parent, "rect", &parent_rect, NULL);

    i3ipcRect *deco_rect = NULL;
    g_object_get(con, "deco_rect", &deco_rect, NULL);

    int win_id;
    g_object_get(con, "id", &win_id, NULL);
    LOG("add window label to window (id: %i)\n", win_id);

    char desc[2];
    desc[0] = curr->num + MIN_CHAR;
    desc[1] = '\0';
    curr->map[curr->num + 0] = win_id;
    curr->num++;

    int x, y;
    if (deco_rect->height == 0) {
        x = rect->x;
        y = rect->y;
    } else {
        x = parent_rect->x + deco_rect->x;
        y = parent_rect->y + deco_rect->y;
    }

    if (xcb_child_window(x, y, desc))
    {
        fprintf(stderr, "error creating child window");
    }

    i3ipc_rect_free(rect);
    i3ipc_rect_free(parent_rect);
    i3ipc_rect_free(deco_rect);
    g_object_unref(parent);
}

static int selection_to_win_id(char_map map, char selection)
{
    if (selection == 0)
    {
        fprintf(stderr, "no selection\n");
        return -1;
    }

    if (selection < MIN_CHAR || selection > MAX_CHAR - 1)
    {
        fprintf(stderr, "selection not in range\n");
        return -1;
    }

    return map.map[selection - MIN_CHAR];
}

int main(void)
{

    if (xcb_init())
    {
        fprintf(stderr, "error initializing xcb");
        return 1;
    }

    if (ipc_init())
    {
        fprintf(stderr, "error initializing i3-ipc");
        return 2;
    }

    GList *cons = ipc_visible_windows();
    if (cons == NULL)
    {
        fprintf(stderr, "error enumerating all visible i3 windows\n");
        return 3;
    }

    char_map map = { .num = 0 };
    int idx;
    for (idx = 0; idx < DIFF_CHAR; idx++)
    {
        map.map[idx] = -1;
    }

    g_list_foreach(cons, (GFunc)create_window_label, &map);
    g_list_free(cons);

    char selection = 0;
    int ret = xcb_main_window("EasyFocus: select a window (press ESC/CR to exit)", &selection);
    if (ret)
    {
        fprintf(stderr, "error creating main window\n");
        return 4;
    }

    LOG("selection: %c\n", selection);
    int win_id = selection_to_win_id(map, selection);
    if (win_id < 0)
    {
        fprintf(stderr, "unknown window label\n");
        return 5;
    }

    LOG("window id: %i\n", win_id);
    if (ipc_focus_window(win_id))
    {
        fprintf(stderr, "cannot issue focus command\n");
        return 6;
    }

    ipc_finish();
    xcb_finish();

    return 0;
}
