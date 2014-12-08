#include "ipc.h"
#include "config.h"
#include "util.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <glib.h>
#include <glib-object.h>

#define BUFFER 512

static i3ipcConnection *connection = NULL;

int ipc_init()
{
    GError *err = NULL;
    connection = i3ipc_connection_new(NULL, &err);
    if (err != NULL)
    {
        fprintf(stderr, "error connecting: %s\n", err->message);
        g_error_free(err);
    }
    return 0;
}

GList *ipc_find_visible_windows(i3ipcCon *root, GList *windows)
{
    const GList *nodes = i3ipc_con_get_nodes(root);
    if (nodes == NULL)
    {
        windows = g_list_append(windows, root);
        return windows;
    }

    gchar *layout = NULL;
    g_object_get(root, "layout", &layout, NULL);
    if (layout == NULL)
    {
        LOG("cannot get layout of con");
        return NULL;
    }

    if ((strncmp(layout, "tabbed", 6) == 0)
            || (strncmp(layout, "stacked", 7) == 0))
    {
        GList *focus_stack = NULL;
        g_object_get(root, "focus", &focus_stack, NULL);
        if (focus_stack == NULL)
        {
            LOG("cannot get focus stack from con");
            g_free(layout);
            return NULL;
        }

        int focus_id = GPOINTER_TO_INT(focus_stack->data);
        const GList *elem;
        i3ipcCon *curr;
        for (elem = nodes; elem; elem = elem->next)
        {
            curr = elem->data;
            int id = -1;
            g_object_get(curr, "id", &id, NULL);
            LOG("%i: %i\n", focus_id, id);
            if (id == focus_id)
            {
                windows = ipc_find_visible_windows(curr, windows);
            }
            else
            {
                windows = g_list_append(windows, curr);
            }

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
            windows = ipc_find_visible_windows(curr, windows);
        }
    }
    else
    {
        LOG("unknown layout of con");
    }

    g_free(layout);
    return windows;
}

GList *ipc_visible_windows()
{
    GError *err = NULL;
    i3ipcCon *tree = i3ipc_connection_get_tree(connection, &err);
    if (err != NULL)
    {
        fprintf(stderr, "error get_tree: %s\n", err->message);
        g_error_free(err);
        return NULL;
    }

    i3ipcCon *focused = i3ipc_con_find_focused(tree);
    i3ipcCon *ws = i3ipc_con_workspace(focused);
    if (ws == NULL)
    {
        fprintf(stderr, "cannot find workspace of focused window\n");
        g_object_unref(tree);
        return NULL;
    }

    GList *cons = ipc_find_visible_windows(ws, NULL);
    g_object_unref(tree);

    return cons;
}

int ipc_focus_window(int win_id)
{
    char *cmd = malloc(BUFFER);
    snprintf(cmd, BUFFER - 1, "[ con_id=%i ] focus", win_id);
    GError *err = NULL;
    i3ipc_connection_command(connection, cmd, &err);
    free(cmd);
    if (err != NULL)
    {
        fprintf(stderr, "focus command returned error: %s\n", err->message);
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


