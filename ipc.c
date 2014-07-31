#include "ipc.h"
#include "config.h"

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
    GList *cons = i3ipc_con_leaves(ws);
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

void ipc_finish() {
    g_object_unref(connection);
    connection = NULL;
}
