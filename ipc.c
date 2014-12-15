#include "ipc.h"
#include "util.h"

#include <string.h>
#include <stdlib.h>
#include <glib.h>
#include <glib-object.h>
#include <i3ipc-glib/i3ipc-glib.h>

#define BUFFER 512

static i3ipcConnection *connection = NULL;

static window *con_to_window(i3ipcCon *con)
{
    int win_id;
    g_object_get(con, "id", &win_id, NULL);

    i3ipcRect *deco_rect = NULL;
    g_object_get(con, "deco_rect", &deco_rect, NULL);

    int x, y;
    if (deco_rect->height == 0)
    {
        i3ipcRect *rect = NULL;
        g_object_get(con, "rect", &rect, NULL);

        x = rect->x;
        y = rect->y;

        i3ipc_rect_free(rect);
    }
    else
    {
        i3ipcCon *parent = NULL;
        g_object_get(con, "parent", &parent, NULL);

        i3ipcRect *parent_rect = NULL;
        g_object_get(parent, "rect", &parent_rect, NULL);

        x = parent_rect->x + deco_rect->x;
        y = parent_rect->y + deco_rect->y;

        i3ipc_rect_free(parent_rect);
        g_object_unref(parent);
    }

    i3ipc_rect_free(deco_rect);

    LOG("found window (id: %i, x: %i, y: %i)\n", win_id, x, y);
    window *window = malloc(sizeof(window));
    window->id = win_id;
    window->position.x = x;
    window->position.y = y;
    window->next = NULL;

    return window;
}

static window *find_visible_windows(i3ipcCon *root)
{
    const GList *nodes = i3ipc_con_get_nodes(root);
    if (nodes == NULL)
    {
        return con_to_window(root);
    }

    gchar *layout = NULL;
    g_object_get(root, "layout", &layout, NULL);

    window *res = NULL;
    if ((strncmp(layout, "tabbed", 6) == 0)
            || (strncmp(layout, "stacked", 7) == 0))
    {
        GList *focus_stack = NULL;
        g_object_get(root, "focus", &focus_stack, NULL);
        if (focus_stack == NULL)
        {
            LOG("empty focus stack in con\n");
            g_free(layout);
            return NULL;
        }

        int focus_id = GPOINTER_TO_INT(focus_stack->data);
        const GList *elem;
        i3ipcCon *curr;
        for (elem = nodes; elem; elem = elem->next)
        {
            curr = elem->data;
            int id;
            g_object_get(curr, "id", &id, NULL);
            window *win = NULL;
            if (id == focus_id)
            {
                win = find_visible_windows(curr);
            }
            else
            {
                win = con_to_window(curr);
            }

            res = window_append(res, win);
        }

        g_list_free(focus_stack);
    }
    else if ((strncmp(layout, "splith", 6) == 0)
             || (strncmp(layout, "splitv", 6) == 0))
    {
        const GList *elem;
        i3ipcCon *curr;
        for (elem = nodes; elem; elem = elem->next)
        {
            curr = elem->data;
            window *win = find_visible_windows(curr);
            res = window_append(res, win);
        }
    }
    else
    {
        LOG("unknown layout of con: %s\n", layout);
    }

    g_free(layout);

    return res;
}

window *ipc_visible_windows()
{
    GError *err = NULL;
    i3ipcCon *tree = i3ipc_connection_get_tree(connection, NULL);
    if (tree == NULL)
    {
        LOG("error getting tree\n");
        g_error_free(err);
        return NULL;
    }

    i3ipcCon *focused = i3ipc_con_find_focused(tree);
    if (focused == NULL)
    {
        LOG("cannot find focused window\n");
        g_object_unref(tree);
        return NULL;
    }

    i3ipcCon *ws = i3ipc_con_workspace(focused);
    if (ws == NULL)
    {
        LOG("cannot find workspace of focused window\n");
        g_object_unref(tree);
        return NULL;
    }

    window *window = find_visible_windows(ws);

    g_object_unref(tree);

    return window;
}

int ipc_focus_window(int win_id)
{
    LOG("focusing window (id: %i)\n", win_id);
    char *cmd = malloc(BUFFER);
    snprintf(cmd, BUFFER - 1, "[ con_id=%i ] focus", win_id);
    GSList *replies = i3ipc_connection_command(connection, cmd, NULL);
    free(cmd);

    i3ipcCommandReply *reply = replies->data;
    if (!reply->success)
    {
        LOG("focus command returned error: %s\n", reply->error);
        g_slist_free_full(replies, (GDestroyNotify) i3ipc_command_reply_free);
        return 1;
    }

    g_slist_free_full(replies, (GDestroyNotify) i3ipc_command_reply_free);

    return 0;
}

int ipc_init()
{
    GError *err = NULL;
    connection = i3ipc_connection_new(NULL, &err);
    if (err != NULL)
    {
        LOG("error connecting: %s\n", err->message);
        g_error_free(err);
        return 1;
    }

    return 0;
}

void ipc_finish()
{
    g_object_unref(connection);
    connection = NULL;
}
