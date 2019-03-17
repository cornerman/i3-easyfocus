#include "ipc.h"
#include "util.h"

#include <string.h>
#include <stdlib.h>
#include <i3ipc-glib/i3ipc-glib.h>

#define BUFFER 512

static i3ipcConnection *connection = NULL;

static Window *con_to_window(i3ipcCon *con)
{
    unsigned long id;
    g_object_get(con, "id", &id, NULL);

    uint32_t win_id;
    g_object_get(con, "window", &win_id, NULL);

    i3ipcRect *deco_rect = NULL;
    g_object_get(con, "deco_rect", &deco_rect, NULL);

    gboolean fullscreen;
    g_object_get(con, "fullscreen-mode", &fullscreen, NULL);

    gboolean urgent;
    g_object_get(con, "urgent", &urgent, NULL);

    gboolean focused;
    g_object_get(con, "focused", &focused, NULL);

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

    LOG("found window (id: %lu, window: %u, x: %i, y: %i)\n", id, win_id, x, y);
    Window *window = malloc(sizeof(Window));
    window->id = id;
    window->win_id = win_id;
    window->position.x = x;
    window->position.y = y;
    window->next = NULL;
    if (urgent) {
        window->type = URGENT_WINDOW;
    } else if (focused) {
        window->type = FOCUSED_WINDOW;
    } else {
        window->type = UNFOCUSED_WINDOW;
    }

    return window;
}

static unsigned long con_get_focused_id(i3ipcCon *con)
{
    GList *focus_stack = NULL;
    g_object_get(con, "focus", &focus_stack, NULL);
    if (focus_stack == NULL)
    {
        LOG("empty focus stack in con\n");
        return 0;
    }

    unsigned long focus_id = (unsigned long) focus_stack->data;

    return focus_id;
}

static i3ipcCon *con_get_visible_container(i3ipcCon *con)
{
    unsigned long id;
    g_object_get(con, "id", &id, NULL);
    LOG("find visible container in con '%lu'\n", id);

    GList *descendants = i3ipc_con_descendents(con);

    i3ipcCon *target = con;
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

    return target;
}

static Window *visible_windows(i3ipcCon *root)
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

    Window *res = NULL;
    const GList *elem;
    if ((strcmp(layout, "tabbed") == 0) ||
        (strcmp(layout, "stacked") == 0))
    {
        unsigned long focus_id = con_get_focused_id(root);
        for (elem = nodes; elem; elem = elem->next)
        {
            i3ipcCon *curr = elem->data;
            unsigned long id;
            g_object_get(curr, "id", &id, NULL);
            Window *win = NULL;
            if (id == focus_id)
            {
                win = visible_windows(curr);
                if (win->id != id)
                {
                    res = window_append(res, con_to_window(curr));
                }
            }
            else
            {
                win = con_to_window(curr);
            }

            res = window_append(res, win);
        }
    }
    else if ((strcmp(layout, "splith") == 0) ||
             (strcmp(layout, "splitv") == 0))
    {
        for (elem = nodes; elem; elem = elem->next)
        {
            i3ipcCon *curr = elem->data;
            res = window_append(res, visible_windows(curr));
        }
    }
    else
    {
        LOG("unknown layout of con: %s\n", layout);
    }

    g_free(layout);
    g_list_free(nodes);

    return res;
}

static Window *visible_windows_on_curr_output(i3ipcCon *root)
{
    i3ipcCon *focused = i3ipc_con_find_focused(root);
    if (focused == NULL)
    {
        LOG("cannot find focused window\n");
        return NULL;
    }

    i3ipcCon *ws = i3ipc_con_workspace(focused);
    ws = (ws == NULL ? focused : ws);

    i3ipcCon *con = con_get_visible_container(ws);
    return visible_windows(con);
}

static gint compare_rects(i3ipcRect *a, i3ipcRect *b)
{
    return a->y == b->y ? a->x - b->x : a->y - b->y;
}

static gint compare_workspace_position(gconstpointer a, gconstpointer b, gpointer outputs_)
{
    const i3ipcWorkspaceReply *reply_a = (i3ipcWorkspaceReply *) a;
    const i3ipcWorkspaceReply *reply_b = (i3ipcWorkspaceReply *) b;

    if (g_str_equal(reply_a->output, reply_b->output))
        return 0;

    i3ipcRect *rect_a = NULL;
    i3ipcRect *rect_b = NULL;
    GSList *outputs = (GSList *) outputs_;
    const GSList *output_reply;
    for (output_reply = outputs; output_reply; output_reply = output_reply->next)
    {
        i3ipcOutputReply *output = output_reply->data;

        if (g_str_equal(output->name, reply_a->output))
            rect_a = output->rect;
        else if (g_str_equal(output->name, reply_b->output))
            rect_b = output->rect;

        if (rect_a && rect_b)
            break;
    }
    if (!rect_a || !rect_b)
    {
        const i3ipcWorkspaceReply *unassigned_workspace = rect_a ? reply_a : reply_b;
        LOG("output %s for workspace %s not found\n", unassigned_workspace->output, unassigned_workspace->name);
        return 0;
    }
    return compare_rects(rect_a, rect_b);
}

static gint compare_workspace_nums(gconstpointer a, gconstpointer b)
{
    const i3ipcWorkspaceReply *reply_a = (i3ipcWorkspaceReply *) a;
    const i3ipcWorkspaceReply *reply_b = (i3ipcWorkspaceReply *) b;

    return reply_a->num - reply_b->num;
}

static Window *visible_windows_on_all_outputs(i3ipcCon *root, SortMethod sort_method)
{
    GSList *raw_replies = i3ipc_connection_get_workspaces(connection, NULL);
    GSList *replies = g_slist_reverse(raw_replies); // i3ipc-glib reverses the order internally

    if (sort_method == BY_NUMBER)
    {
        replies = g_slist_sort(replies, compare_workspace_nums);
    }
    else if (sort_method == BY_LOCATION)
    {
        GSList *outputs = i3ipc_connection_get_outputs(connection, NULL);
        replies = g_slist_sort_with_data(replies, compare_workspace_position, outputs);
        g_slist_free_full(outputs, (GDestroyNotify) i3ipc_output_reply_free);
    }

    GList *workspaces = i3ipc_con_workspaces(root);

    Window *res = NULL;
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
                i3ipcCon *con = con_get_visible_container(curr_ws);
                res = window_append(res, visible_windows(con));
                break;
            }
        }
    }

    g_slist_free_full(replies, (GDestroyNotify) i3ipc_workspace_reply_free);
    g_list_free(workspaces);

    return res;
}

static Window *visible_windows_in_curr_con(i3ipcCon *root)
{
    i3ipcCon *focused = i3ipc_con_find_focused(root);
    if (focused == NULL)
    {
        LOG("cannot find focused window\n");
        return NULL;
    }

    i3ipcCon *parent;
    g_object_get(focused, "parent", &parent, NULL);
    i3ipcCon *con = con_get_visible_container(parent);
    g_object_unref(parent);

    return visible_windows(con);
}

Window *ipc_visible_windows(SearchArea search_area, SortMethod sort_method)
{
    i3ipcCon *root = i3ipc_connection_get_tree(connection, NULL);
    if (root == NULL)
    {
        LOG("error getting tree\n");
        return NULL;
    }

    Window *windows = NULL;
    switch (search_area)
    {
    case CURRENT_OUTPUT:
        windows = visible_windows_on_curr_output(root);
        break;
    case ALL_OUTPUTS:
        windows = visible_windows_on_all_outputs(root, sort_method);
        break;
    case CURRENT_CONTAINER:
        windows = visible_windows_in_curr_con(root);
        break;
    }

    g_object_unref(root);

    return windows;
}

int ipc_focus_window(Window *window)
{
    LOG("focusing window (id: %lu)\n", window->id);
    char *cmd = malloc(BUFFER);
    snprintf(cmd, BUFFER - 1, "[ con_id=%lu ] focus", window->id);
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
