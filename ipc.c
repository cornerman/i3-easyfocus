#include "ipc.h"
#include "all.h"

#include <glib.h>
#include <glib-object.h>

static i3ipcConnection *connection = NULL;

int ipc_init()
{
    GError *err = NULL;
    connection = i3ipc_connection_new(NULL, &err);
    if (err != NULL)
    {
        fprintf(stderr, "error: %s\n", err->message);
        g_error_free(err);
    }
    return 0;
}

i3ipcConnection *ipc_connection()
{
    return connection;
}

void ipc_finish() {
    g_object_unref(connection);
    connection = NULL;
}
