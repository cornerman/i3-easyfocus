#include "util.h"
#include "ipc.h"
#include "xcb.h"
#include "map.h"

static int create_window_label(window *win)
{
    char key = map_add(win->id);
    if (key < 0)
    {
        LOG("cannot add window id\n");
        return 1;
    }

    if (xcb_register_for_key_event(key))
    {
        LOG("cannot register for key event\n");
        return 1;
    }

    char desc[2];
    desc[0] = key;
    desc[1] = '\0';
    if (xcb_create_text_window(win->position.x, win->position.y, desc))
    {
        LOG("error creating text window\n");
        return 1;
    }

    return 0;
}

static int create_window_labels(window *win)
{
    map_init();
    while (win != NULL) {
        if (create_window_label(win))
        {
            return 1;
        }

        win = win->next;
    }

    return 0;
}

int main(void)
{
    if (xcb_init())
    {
        fprintf(stderr, "error initializing xcb\n");
        return 1;
    }

    if (ipc_init())
    {
        fprintf(stderr, "error initializing ipc\n");
        return 1;
    }

    window *win = ipc_visible_windows();
    if (win == NULL)
    {
        fprintf(stderr, "no visible windows\n");
        return 1;
    }

    int created = create_window_labels(win);
    window_free(win);
    if (created)
    {
        fprintf(stderr, "error creating window labels\n");
        return 1;
    }

    char selection;
    if (xcb_wait_for_key_event(&selection))
    {
        fprintf(stderr, "no selection\n");
        return 1;
    }

    LOG("selection: %c\n", selection);
    int win_id = map_get(selection);
    if (win_id < 0)
    {
        fprintf(stderr, "unknown window label\n");
        return 1;
    }

    LOG("window id: %i\n", win_id);
    if (ipc_focus_window(win_id))
    {
        fprintf(stderr, "cannot focus window\n");
        return 1;
    }

    ipc_finish();
    xcb_finish();

    return 0;
}
