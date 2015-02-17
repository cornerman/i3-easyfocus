#include "util.h"
#include "ipc.h"
#include "xcb.h"
#include "map.h"

static int print_id = 0;
static SearchArea search_area = CURRENT_OUTPUT;

int parse_args(int argc, char *argv[])
{
    while ((argc > 1) && (argv[1][0] == '-'))
    {
        switch (argv[1][1])
        {
        case 'p':
            print_id = 1;
            break;
        case 'a':
            search_area = ALL_OUTPUTS;
            break;
        default:
            printf("Usage: i3-easyfocus <options>\n");
            printf(" -p    only print con id\n");
            printf(" -a    label visible windows on all outputs\n");
            return 1;
        }

        ++argv;
        --argc;
    }
    return 0;
}

static int create_window_label(Window *win)
{
    char key = map_add(win->id);
    if (key < 0)
    {
        fprintf(stderr, "cannot add window id\n");
        return 1;
    }

    if (xcb_register_for_key_event(key))
    {
        fprintf(stderr, "cannot register for key event\n");
        return 1;
    }

    char desc[2];
    desc[0] = key;
    desc[1] = '\0';
    if (xcb_create_text_window(win->position.x, win->position.y, desc))
    {
        fprintf(stderr, "error creating text window\n");
        return 1;
    }

    return 0;
}

static int create_window_labels()
{
    Window *win = ipc_visible_windows(search_area);
    if (win == NULL)
    {
        fprintf(stderr, "no visible windows\n");
        return 1;
    }

    map_init();
    Window *curr = win;
    while (curr != NULL)
    {
        if (create_window_label(curr))
        {
            window_free(win);
            return 1;
        }

        curr = curr->next;
    }

    window_free(win);
    return 0;
}

static int handle_selection()
{
    char selection;
    if (xcb_wait_for_key_event(&selection))
    {
        fprintf(stderr, "no selection\n");
        return 1;
    }

    LOG("selection: %c\n", selection);
    int id = map_get(selection);
    if (id < 0)
    {
        fprintf(stderr, "unknown selection\n");
        return 1;
    }

    LOG("window id: %i\n", id);
    if (print_id)
    {
        printf("%i\n", id);
    }
    else if (ipc_focus_window(id))
    {
        fprintf(stderr, "cannot focus window\n");
        return 1;
    }

    return 0;
}

int main(int argc, char *argv[])
{
    if (parse_args(argc, argv))
    {
        return 1;
    }

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

    if (create_window_labels())
    {
        return 1;
    }

    if (handle_selection())
    {
        return 1;
    }

    ipc_finish();
    xcb_finish();

    return 0;
}
