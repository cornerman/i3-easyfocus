#include "xcb.h"
#include "util.h"
#include "config.h"

#include <stdlib.h>
#include <string.h>

#include <xcb/xcb.h>
#include <xcb/xproto.h>
#include <xcb/xcb_keysyms.h>

#define XK_MISCELLANY
#include <X11/keysymdef.h>

enum
{
    _NET_WM_WINDOW_TYPE,
    _NET_WM_WINDOW_TYPE_DIALOG,
    NUM_ATOMS
};

static xcb_atom_t atoms[NUM_ATOMS];

static xcb_connection_t *connection = NULL;
static xcb_screen_t *screen = NULL;

static int get_atom(char *name, uint16_t type)
{
    xcb_intern_atom_cookie_t cookie = xcb_intern_atom(connection, 0, strlen(name), name);
    xcb_intern_atom_reply_t *reply = xcb_intern_atom_reply(connection, cookie, NULL);
    if (reply == NULL)
    {
        LOG("unable to get xcb atom\n");
        return 1;
    }

    atoms[type] = reply->atom;
    free(reply);

    return 0;
}

static int get_atoms()
{
#define GET_ATOM(name)           \
    if (get_atom(#name, name)) { \
        return 1;                \
    }

    GET_ATOM(_NET_WM_WINDOW_TYPE);
    GET_ATOM(_NET_WM_WINDOW_TYPE_DIALOG);
#undef GET_ATOM

    return 0;
}

static int request_failed(xcb_void_cookie_t cookie, char *err_msg)
{
    xcb_generic_error_t *err;
    if ((err = xcb_request_check(connection, cookie)) != NULL)
    {
        LOG("request failed: %s. error code: %d\n", err_msg, err->error_code);
        free(err);
        return 1;
    }

    return 0;
}

static int draw_text(xcb_window_t window, int16_t x, int16_t y, const char *label)
{
    xcb_font_t font = xcb_generate_id(connection);
    xcb_void_cookie_t font_cookie = xcb_open_font_checked(connection,
                                    font,
                                    strlen(XCB_FONT_NAME),
                                    XCB_FONT_NAME);

    if (request_failed(font_cookie, "cannot open font"))
    {
        return 1;
    }

    xcb_gcontext_t gc = xcb_generate_id(connection);
    uint32_t mask = XCB_GC_FOREGROUND | XCB_GC_BACKGROUND | XCB_GC_FONT;
    uint32_t value_list[3] = { screen->black_pixel,
                               screen->white_pixel,
                               font
                             };

    xcb_void_cookie_t gc_cookie = xcb_create_gc_checked(connection,
                                  gc,
                                  window,
                                  mask,
                                  value_list);

    if (request_failed(gc_cookie, "cannot open gc"))
    {
        return 1;
    }

    font_cookie = xcb_close_font_checked(connection, font);
    if (request_failed(font_cookie, "cannot close font"))
    {
        return 1;
    }

    xcb_void_cookie_t text_cookie = xcb_image_text_8_checked(connection,
                                    strlen(label),
                                    window,
                                    gc,
                                    x, y,
                                    label);

    if (request_failed(text_cookie, "cannot paste text"))
    {
        return 1;
    }

    xcb_void_cookie_t gcfree_cookie = xcb_free_gc(connection, gc);
    if (request_failed(gcfree_cookie, "cannot free gc"))
    {
        return 1;
    }

    return 0;
}

int xcb_child_window(int pos_x, int pos_y, char *desc)
{
    uint32_t mask;
    uint32_t values[2];

    xcb_window_t window = xcb_generate_id(connection);
    mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
    values[0] = screen->white_pixel;
    values[1] = XCB_EVENT_MASK_EXPOSURE;

    LOG("create window (id: %i, x: %i, y: %i): %s\n", window, pos_x, pos_y, desc);
    xcb_void_cookie_t window_cookie = xcb_create_window_checked(connection,
                                      screen->root_depth,
                                      window, screen->root,
                                      pos_x, pos_y,
                                      XCB_CHILD_WIDTH, XCB_CHILD_HEIGHT,
                                      0, XCB_WINDOW_CLASS_INPUT_OUTPUT,
                                      screen->root_visual,
                                      mask, values);

    if (request_failed(window_cookie, "cannot create window"))
    {
        return 1;
    }

    mask = XCB_CW_OVERRIDE_REDIRECT;
    values[0] = 1;
    xcb_void_cookie_t or_cookie = xcb_change_window_attributes(connection, window, mask, values);
    if (request_failed(or_cookie, "cannot set override redirect"))
    {
        return 1;
    }

    xcb_void_cookie_t map_cookie = xcb_map_window_checked(connection, window);
    if (request_failed(map_cookie, "cannot map window"))
    {
        return 1;
    }

    xcb_flush(connection);

    xcb_generic_event_t  *event;
    while (1)
    {
        if ((event = xcb_poll_for_event(connection)))
        {
            switch (event->response_type & ~0x80)
            {
            case XCB_EXPOSE:
            {
                free(event);
                if (draw_text(window, 4, XCB_CHILD_HEIGHT - 2, desc))
                {
                    LOG("error drawing text\n");
                    return 1;
                }

                return 0;
            }
            }

            free(event);
        }
    }

    return 1;
}

int xcb_main_window(char *keysym_out)
{
    uint32_t mask;
    uint32_t values[2];

    xcb_window_t window = xcb_generate_id(connection);
    mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
    values[0] = screen->white_pixel;
    values[1] = XCB_EVENT_MASK_KEY_PRESS |
                XCB_EVENT_MASK_EXPOSURE;

    LOG("create main  window (id: %i)\n", window);
    xcb_void_cookie_t window_cookie = xcb_create_window_checked(connection,
                                      screen->root_depth,
                                      window, screen->root,
                                      XCB_MAIN_POS_X, XCB_MAIN_POS_Y,
                                      XCB_MAIN_WIDTH, XCB_MAIN_HEIGHT,
                                      0, XCB_WINDOW_CLASS_INPUT_OUTPUT,
                                      screen->root_visual,
                                      mask, values);

    if (request_failed(window_cookie, "cannot create window"))
    {
        return 1;
    }


    xcb_void_cookie_t dialog_cookie = xcb_change_property(connection,
                                      XCB_PROP_MODE_REPLACE,
                                      window,
                                      atoms[_NET_WM_WINDOW_TYPE],
                                      XCB_ATOM_ATOM,
                                      32,
                                      1,
                                      (unsigned char *)&atoms[_NET_WM_WINDOW_TYPE_DIALOG]);

    if (request_failed(dialog_cookie, "cannot set _NET_WM_WINDOW_TYPE"))
    {
        return 1;
    }

    char *title = "easyfocus";
    xcb_void_cookie_t title_cookie = xcb_change_property(connection,
                                     XCB_PROP_MODE_REPLACE,
                                     window,
                                     XCB_ATOM_WM_NAME,
                                     XCB_ATOM_STRING,
                                     8,
                                     strlen(title),
                                     title);

    if (request_failed(title_cookie, "cannot set title"))
    {
        return 1;
    }

    xcb_void_cookie_t map_cookie = xcb_map_window_checked(connection, window);
    if (request_failed(map_cookie, "cannot map window"))
    {
        return 1;
    }

    xcb_flush(connection);

    xcb_generic_event_t  *event;
    while (1)
    {
        if ((event = xcb_poll_for_event(connection)))
        {
            switch (event->response_type & ~0x80)
            {
            case XCB_EXPOSE:
            {
                LOG("xcb expose event\n");
                if (draw_text(window, 4, XCB_MAIN_HEIGHT - 2, XCB_MAIN_TEXT))
                {
                    LOG("error drawing text\n");
                    free(event);
                    return 1;
                }
            }
            case XCB_KEY_PRESS:
            {
                xcb_key_press_event_t *kr = (xcb_key_press_event_t *)event;

                LOG("key: %i\n", kr->detail);
                xcb_key_symbols_t *syms = xcb_key_symbols_alloc(connection);
                xcb_keysym_t keysym = xcb_key_press_lookup_keysym(syms, kr, 0);
                LOG("sym: %i\n", keysym);

                xcb_key_symbols_free(syms);
                free(event);

                if (keysym == 0)
                {
                    continue;
                }

                if (keysym == XK_Escape || keysym == XK_Return)
                {
                    LOG("escape/return pressed\n");
                    return 0;
                }

                *keysym_out = keysym;
                return 0;
            }
            }

            free(event);
        }
    }

    return 1;
}

int xcb_init()
{
    connection = xcb_connect(NULL, NULL);
    screen = xcb_setup_roots_iterator(xcb_get_setup(connection)).data;
    if (get_atoms())
    {
        LOG("error getting atoms\n");
        return 1;
    }

    return 0;
}

void xcb_finish()
{
    xcb_disconnect(connection);
}
