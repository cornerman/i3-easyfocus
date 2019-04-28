#include "ipc.h"
#include "xcb.h"
#include "map.h"
#include "util.h"
#include "color_config.h"
#include "config.h"

#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <getopt.h>
#include <X11/keysym.h>
#include <X11/keysymdef.h>

static int print_id = 0;
static int window_id = 0;
static int rapid_mode = 0;
static SearchArea search_area = CURRENT_OUTPUT;
static SortMethod sort_method = BY_LOCATION;
static char *font_name = XCB_DEFAULT_FONT_NAME;
static label_key_mode_e key_mode = LABEL_KEY_MODE_DEFAULT;
static ColorConfig color_config = { COLOR_DEFAULT_URGENT_BG, COLOR_DEFAULT_URGENT_FG, COLOR_DEFAULT_FOCUSED_BG, COLOR_DEFAULT_FOCUSED_FG, COLOR_DEFAULT_UNFOCUSED_BG, COLOR_DEFAULT_UNFOCUSED_FG };
static uint16_t modifier_mask = 0;

static void print_help(void)
{
    fprintf(stderr, "Usage: i3-easyfocus <options>\n");
    fprintf(stderr, " -h --help              show this message\n");
    fprintf(stderr, " -i --con-id            print con id, does not change focus\n");
    fprintf(stderr, " -w --window-id         print window id, does not change focus\n");
    fprintf(stderr, " -a --all               label visible windows on all outputs\n");
    fprintf(stderr, " -c --current           label visible windows within current container\n");
    fprintf(stderr, " -r --rapid             rapid mode, keep on running until Escape is pressed\n");
    fprintf(stderr, " -m --modifier <mod>    listen to keycombo <mod>+<label> instead of only <label>\n");
    fprintf(stderr, "                            - ctrl, shift, mod1, mod2, mod3, mod4, mod5\n");
    fprintf(stderr, "                            - or combine with, e.g., mod1+shift\n");
    fprintf(stderr, " -s --sort-by <method>  how to sort the workspaces' labels when using --all:\n");
    fprintf(stderr, "                            - <location> based on their location (default)\n");
    fprintf(stderr, "                            - <num> using the workspaces' numbers\n");
    fprintf(stderr, " -f --font <font-name>  set font name, see `xlsfonts` for available fonts\n");
    fprintf(stderr, " -k --keys <mode>       set the labeling keys to use:\n");
    fprintf(stderr, "                            - <avy> prefers home row for qwerty (default)\n");
    fprintf(stderr, "                            - <colemak> prefers home row for colemak\n");
    fprintf(stderr, "                            - <alpha> orders alphabetically\n");
    fprintf(stderr, " --color-urgent-bg <rgb>    set label background color of urgent windows, e.g., FF00FF\n");
    fprintf(stderr, " --color-focused-bg <rgb>   set label background color of focused windows, e.g., FF00FF\n");
    fprintf(stderr, " --color-unfocused-bg <rgb> set label background color of unfocused windows, e.g., FF00FF\n");
    fprintf(stderr, " --color-urgent-fg <rgb>    set label foreground color of urgent windows, e.g., FF00FF\n");
    fprintf(stderr, " --color-focused-fg <rgb>   set label foreground color of focused windows, e.g., FF00FF\n");
    fprintf(stderr, " --color-unfocused-fg <rgb> set label foreground color of unfocused windows, e.g., FF00FF\n");
}

static void parse_args(int argc, char *argv[])
{
    static struct option long_options[] = {
        {"con-id", no_argument, 0, 'i'},
        {"window-id", no_argument, 0, 'w'},
        {"all", no_argument, 0, 'a'},
        {"current", no_argument, 0, 'c'},
        {"rapid", no_argument, 0, 'r'},
        {"font", required_argument, 0, 'f'},
        {"modifier", required_argument, 0, 'm'},
        {"color-urgent-bg", required_argument, 0, 1000},
        {"color-focused-bg", required_argument, 0, 1001},
        {"color-unfocused-bg", required_argument, 0, 1002},
        {"color-urgent-fg", required_argument, 0, 1003},
        {"color-focused-fg", required_argument, 0, 1004},
        {"color-unfocused-fg", required_argument, 0, 1005},
        {"help", no_argument, 0, 'h'},
        {"keys", required_argument, 0, 'k'},
        {0, 0, 0, 0}};
    char *options_string = "iwacrf:s:hk:m:";
    int o, option_index;

    bool got_sort_method = false;
    while ((o = getopt_long(argc, argv, options_string, long_options, &option_index)) != -1)
    {
        switch (o)
        {
        case 'h':
            print_help();
            exit(0);
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
            font_name = strdup(optarg);
            break;
        case 'm':
            modifier_mask = xcb_modifier_string_to_mask(optarg);
            break;
        case 'k':
            if (strcmp(optarg, "avy") == 0)
            {
                key_mode = LABEL_KEY_MODE_AVY;
            }
            else if (strcmp(optarg, "alpha") == 0)
            {
                key_mode = LABEL_KEY_MODE_ALPHA;
            }
            else if (strcmp(optarg, "colemak") == 0)
            {
                key_mode = LABEL_KEY_MODE_COLEMAK;
            }
            else
            {
                fprintf(stderr, "unrecognized key style type: %s\n", optarg);
                print_help();
                exit(EXIT_FAILURE);
            }
            break;
        case 's':
            got_sort_method = true;
            if (strcmp(optarg, "num") == 0)
            {
                sort_method = BY_NUMBER;
                break;
            }
            else if (strcmp(optarg, "location") == 0)
            {
                sort_method = BY_LOCATION;
                break;
            }
            else
            {
                fprintf(stderr, "wrong type of sort method: %s\n", optarg);
                print_help();
                exit(EXIT_FAILURE);
            }
        case 1000:
            if (parse_rgb_string(optarg, &(color_config.urgent_bg))) {
                fprintf(stderr, "cannot parse rgb string: %s\n", optarg);
                exit(EXIT_FAILURE);
            }
            break;
        case 1001:
            if (parse_rgb_string(optarg, &(color_config.focused_bg))) {
                fprintf(stderr, "cannot parse rgb string: %s\n", optarg);
                exit(EXIT_FAILURE);
            }
            break;
        case 1002:
            if (parse_rgb_string(optarg, &(color_config.unfocused_bg))) {
                fprintf(stderr, "cannot parse rgb string: %s\n", optarg);
                exit(EXIT_FAILURE);
            }
            break;
        case 1003:
            if (parse_rgb_string(optarg, &(color_config.urgent_fg))) {
                fprintf(stderr, "cannot parse rgb string: %s\n", optarg);
                exit(EXIT_FAILURE);
            }
            break;
        case 1004:
            if (parse_rgb_string(optarg, &(color_config.focused_fg))) {
                fprintf(stderr, "cannot parse rgb string: %s\n", optarg);
                exit(EXIT_FAILURE);
            }
            break;
        case 1005:
            if (parse_rgb_string(optarg, &(color_config.unfocused_fg))) {
                fprintf(stderr, "cannot parse rgb string: %s\n", optarg);
                exit(EXIT_FAILURE);
            }
            break;
        default:
            print_help();
            exit(EXIT_FAILURE);
        }
    }
    if (got_sort_method && search_area != ALL_OUTPUTS)
        fprintf(stderr, "warning: ignoring provided --sort-by argument, use the --all flag.\n");
}

static int create_window_label(Window *win)
{
    xcb_keysym_t key = map_add(win);
    if (key == XCB_NO_SYMBOL)
    {
        fprintf(stderr, "cannot find a free keysym\n");
        return 1;
    }

    if (xcb_grab_keysym(key, modifier_mask))
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

    if (xcb_create_text_window(win->position.x, win->position.y, win->type, label))
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
    map_init(key_mode);
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
    if (xcb_init(font_name, color_config))
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

    if (xcb_grab_keysym(EXIT_KEYSYM, modifier_mask))
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

        Window *win = ipc_visible_windows(search_area, sort_method);
        if (win == NULL)
        {
            fprintf(stderr, "no visible windows\n");
            return 1;
        }

        if (create_window_labels(win))
        {
            map_free();
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
            }
            else if (selection != EXIT_KEYSYM)
            {
                if (handle_selection(selection))
                {
                    map_free();
                    window_free(win);
                    return 1;
                }

                searching = rapid_mode;
            }
        }

        map_free();
        window_free(win);
    }

    return 0;
}

int main(int argc, char *argv[])
{
    parse_args(argc, argv);

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
