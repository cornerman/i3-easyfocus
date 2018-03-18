#include "ipc.h"
#include "xcb.h"
#include "map.h"
#include "util.h"
#include "config.h"

#include <stdlib.h>
#include <X11/keysym.h>
#include <X11/keysymdef.h>

static int print_id = 0;
static int window_id = 0;
static int rapid_mode = 0;
static SearchArea search_area = CURRENT_OUTPUT;
static char* font_name = XCB_DEFAULT_FONT_NAME;

static void print_help() {
    printf("Usage: i3-easyfocus <options>\n");
    printf(" -h                 show this message\n");
    printf(" -i                 print con id, does not change focus\n");
    printf(" -w                 print window id, does not change focus\n");
    printf(" -a                 label visible windows on all outputs\n");
    printf(" -c                 label visible windows within current container\n");
    printf(" -r                 rapid mode, keep on running until Escape is pressed\n");
    printf(" -f <font-name>     set font name, see `xlsfonts` for available fonts\n");
}

static int parse_args(int argc, char *argv[])
{
    while ((argc > 1) && (argv[1][0] == '-'))
    {
        switch (argv[1][1])
        {
        case 'h':
            return 1;
        case 'i':
            print_id = 1;
            window_id = 0;
            break;
        case 'w':
            print_id = 1;
            window_id = 1;
            break;
        case 'a':
            search_area = ALL_OUTPUTS;
            break;
        case 'c':
            search_area = CURRENT_CONTAINER;
            break;
        case 'r':
            rapid_mode = 1;
            break;
        case 'f':
            if (argc > 2) {
                font_name = argv[2];
                ++argv;
                --argc;
                break;
            } else {
                fprintf(stderr, "option '-f' needs argument <font-name>\n");
                return 1;
            }
        default:
            fprintf(stderr, "unknown option '-%c'\n", argv[1][1]);
            return 1;
        }

        ++argv;
        --argc;
    }
    return 0;
}

static int create_window_label(Window *win)
{
    xcb_keysym_t key = map_add(win);
    if (key == XCB_NO_SYMBOL)
    {
        fprintf(stderr, "cannot find a free keysym\n");
        return 1;
    }

    if (xcb_grab_keysym(key))
    {
        fprintf(stderr, "cannot register for key event\n");
        return 1;
    }

    char *label = xcb_keysym_to_string(key);
    if (label == NULL)
    {
        fprintf(stderr, "cannot convert keysym to string\n");
        return 1;
    }

    if (xcb_create_text_window(win->position.x, win->position.y, label))
    {
        fprintf(stderr, "cannot create text window\n");
        free(label);
        return 1;
    }

    free(label);
    return 0;
}

static int create_window_labels(Window *win)
{
    map_init();
    Window *curr = win;
    while (curr != NULL)
    {
        if (create_window_label(curr))
        {
            return 1;
        }

        curr = curr->next;
    }

    return 0;
}

static int handle_selection(xcb_keysym_t selection)
{
    Window *win = map_get(selection);
    if (win == NULL)
    {
        fprintf(stderr, "unknown selection\n");
        return 1;
    }

    LOG("window (id: %lu, window: %u)\n", win->id, win->win_id);
    if (print_id)
    {
        if (window_id)
            printf("%u\n", win->win_id);
        else
            printf("%lu\n", win->id);
    }
    else if (ipc_focus_window(win))
    {
        fprintf(stderr, "cannot focus window\n");
        return 1;
    }

    return 0;
}

static int setup_xcb()
{
    if (xcb_init(font_name))
    {
        fprintf(stderr, "error initializing xcb\n");
        return 1;
    }

    if (xcb_register_configure_notify())
    {
        xcb_finish();
        fprintf(stderr, "failed to register for configure notify\n");
        return 1;
    }

    if (xcb_grab_keysym(EXIT_KEYSYM))
    {
        xcb_finish();
        fprintf(stderr, "cannot grab exit keysym\n");
        return 1;
    }

    return 0;
}

static int select_window()
{
    int searching = 1;
    while (searching)
    {
        // TODO: should probably not reconnect to xserver each time
        // - need to destroy all previous popup windows.
        if (setup_xcb())
        {
            return 1;
        }

        Window *win = ipc_visible_windows(search_area);
        if (win == NULL)
        {
            fprintf(stderr, "no visible windows\n");
            return 1;
        }

        if (create_window_labels(win))
        {
            window_free(win);
            return 1;
        }

        xcb_keysym_t selection = xcb_wait_for_user_input();
        xcb_finish();

        if (selection != XCB_NO_SYMBOL)
        {
            LOG("selection: %i\n", selection);
            if (selection == EXIT_KEYSYM)
            {
                searching = 0;
            } else if (selection != EXIT_KEYSYM)
            {
                if (handle_selection(selection))
                {
                    window_free(win);
                    return 1;
                }

                searching = rapid_mode;
            }

        }

        window_free(win);
    }

    return 0;
}

int main(int argc, char *argv[])
{
    if (parse_args(argc, argv))
    {
        print_help();
        return 1;
    }

    if (ipc_init())
    {
        fprintf(stderr, "error initializing ipc\n");
        return 1;
    }

    if (select_window())
    {
        return 1;
    }

    ipc_finish();

    return 0;
}

