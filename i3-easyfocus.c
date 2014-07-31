#include "all.h"
#include "ipc.h"
#include "xcb.h"

#include <glib.h>
#include <glib-object.h>

#define MIN_CHAR 97
#define MAX_CHAR 122
#define DIFF_CHAR MAX_CHAR - MIN_CHAR

typedef struct char_map_t
{
    int map[DIFF_CHAR];
    char num;
} char_map;

void create_window_label(gpointer item, gpointer data)
{
    i3ipcCon *con = item;
    char_map *curr = data;
    if (curr->num > 122)
    {
        fprintf(stderr, "too many windows, we only have 26 characters\n");
        return;
    }

    i3ipcRect *rect = NULL;
    g_object_get(con, "rect", &rect, NULL);

    int win_id;
    g_object_get(con, "id", &win_id, NULL);
    LOG("window - id:%i, x:%i, y:%i\n", win_id, rect->x, rect->y);

    char desc[2];
    desc[0] = curr->num;
    desc[1] = '\0';
    curr->map[curr->num - MIN_CHAR] = win_id;
    curr->num++;

    if (xcb_child_window(rect->x + 20, rect->y + 20, desc))
    {
        fprintf(stderr, "error creating child window");
    }

    i3ipc_rect_free(rect);
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
        xcb_finish();
        return 2;
    }

    i3ipcConnection *conn = ipc_connection();
    i3ipcCon *tree = i3ipc_connection_get_tree(conn, NULL);
    i3ipcCon *focused = i3ipc_con_find_focused(tree);
    i3ipcCon *ws = i3ipc_con_workspace(focused);

    GList *cons = i3ipc_con_leaves(ws);
    char_map map = { .num = MIN_CHAR };
    int idx;
    for (idx = 0; idx < DIFF_CHAR; idx++)
    {
        map.map[idx] = -1;
    }

    g_list_foreach(cons, create_window_label, &map);

    g_list_free(cons);
    g_object_unref(tree);

    char selection = 0;
    int ret = xcb_main_window("EasyFocus: select a window (press ESC/CR to exit)", &selection);

    ipc_finish();
    xcb_finish();

    if (ret)
    {
        fprintf(stderr, "error creating main window");
        return 4;
    }

    if (selection == 0)
    {
        fprintf(stderr, "no selection\n");
        return 0;
    }

    LOG("selection: %c\n", selection);
    if (selection < MIN_CHAR || selection > MAX_CHAR - 1)
    {
        fprintf(stderr, "selection not in range\n");
        return 5;
    }

    printf("%i", map.map[selection - MIN_CHAR]);

    return 0;
}
