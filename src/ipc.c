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

    gboolean fullscreen;
    g_object_get(con, "fullscreen-mode", &fullscreen, NULL);

    int x, y;
    if (fullscreen || (deco_rect->height == 0))
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

static int con_get_focused_id(i3ipcCon *con)
{
    GList *focus_stack = NULL;
    g_object_get(con, "focus", &focus_stack, NULL);
    if (focus_stack == NULL)
    {
        LOG("empty focus stack in con\n");
        return -1;
    }

    int focus_id = GPOINTER_TO_INT(focus_stack->data);
    g_list_free(focus_stack);

    return focus_id;
}

static window *visible_windows(i3ipcCon *root)
{
    GList *nodes = g_list_copy((GList *) i3ipc_con_get_nodes(root));
    GList *floating = g_list_copy((GList *) i3ipc_con_get_floating_nodes(root));
    nodes = g_list_concat(nodes, floating);
    if (nodes == NULL)
    {
        return con_to_window(root);
    }

    gchar *layout = NULL;
    g_object_get(root, "layout", &layout, NULL);

    window *res = NULL;
    const GList *elem;
    i3ipcCon *curr;
    if ((strncmp(layout, "tabbed", 6) == 0)
            || (strncmp(layout, "stacked", 7) == 0))
    {
        int focus_id = con_get_focused_id(root);
        for (elem = nodes; elem; elem = elem->next)
        {
            curr = elem->data;
            int id;
            g_object_get(curr, "id", &id, NULL);
            window *win = NULL;
            if (id == focus_id)
            {
                win = visible_windows(curr);
            }
            else
            {
                win = con_to_window(curr);
            }

            res = window_append(res, win);
        }
    }
    else if ((strncmp(layout, "splith", 6) == 0)
             || (strncmp(layout, "splitv", 6) == 0))
    {
        for (elem = nodes; elem; elem = elem->next)
        {
            curr = elem->data;
            window *win = visible_windows(curr);
            res = window_append(res, win);
        }
    }
    else
    {
        LOG("unknown layout of con: %s\n", layout);
    }

    g_free(layout);
    g_list_free(nodes);
    g_list_free(floating);

    return res;
}

static window* visible_windows_on_ws(i3ipcCon *ws)
{
    const char *name = i3ipc_con_get_name(ws);
    LOG("find visible windows on ws '%s'\n", name);

    i3ipcCon *target = ws;
    GList *descendants = i3ipc_con_descendents(ws);
    GList *elem = NULL;
    i3ipcCon *curr = NULL;
    for (elem = descendants; elem; elem = elem->next)
    {
        curr = elem->data;
        gboolean fullscreen;
        g_object_get(curr, "fullscreen-mode", &fullscreen, NULL);
        if (fullscreen)
        {
            LOG("con is in fullscreen mode\n");
            target = curr;
            break;
        }
    }

    g_list_free(descendants);

    return visible_windows(target);
}

static window* visible_windows_on_curr_ws(i3ipcCon *root)
{
    i3ipcCon *focused = i3ipc_con_find_focused(root);
    if (focused == NULL)
    {
        LOG("cannot find focused window\n");
        return NULL;
    }

    i3ipcCon *ws = i3ipc_con_workspace(focused);
    ws = ((ws == NULL) ? focused : ws);

    return visible_windows_on_ws(ws);
}

static window* visible_windows_on_all_ws(i3ipcCon *root)
{
    GSList *replies = i3ipc_connection_get_workspaces(connection, NULL);
    GList *workspaces = i3ipc_con_workspaces(root);
    window *res = NULL;

    const GSList *reply;
    i3ipcWorkspaceReply *curr_reply;
    for (reply = replies; reply; reply = reply->next)
    {
        curr_reply = reply->data;
        if (!curr_reply->visible)
            continue;

        const GList *ws;
        i3ipcCon *curr_ws;
        for (ws = workspaces; ws; ws = ws->next)
        {
            curr_ws = ws->data;
            const char *name = i3ipc_con_get_name(curr_ws);
            if (strcmp(curr_reply->name, name) == 0)
            {
                window *window = visible_windows_on_ws(curr_ws);
                res = window_append(res, window);
                break;
            }
        }
    }

    g_slist_free_full(replies, (GDestroyNotify) i3ipc_workspace_reply_free);
    g_list_free(workspaces);

    return res;
}

window *ipc_visible_windows(int visible_ws)
{
    GError *err = NULL;
    i3ipcCon *root = i3ipc_connection_get_tree(connection, NULL);
    if (root == NULL)
    {
        LOG("error getting tree\n");
        g_error_free(err);
        return NULL;
    }

    window *windows = NULL;
    if (visible_ws)
    {
        windows = visible_windows_on_all_ws(root);
    }
    else
    {
        windows = visible_windows_on_curr_ws(root);
    }

    g_object_unref(root);

    return windows;
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
