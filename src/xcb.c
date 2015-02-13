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

static xcb_connection_t *connection = NULL;
static xcb_screen_t *screen = NULL;
static xcb_key_symbols_t *keysyms = NULL;

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

static int grab_keysym(xcb_keysym_t keysym)
{
    xcb_keycode_t min_keycode = xcb_get_setup(connection)->min_keycode;
    xcb_keycode_t max_keycode = xcb_get_setup(connection)->max_keycode;

    xcb_keycode_t i;
    for (i = min_keycode; i && i < max_keycode; i++)
    {
        if ((xcb_key_symbols_get_keysym(keysyms, i, 0) != keysym))
        {
            continue;
        }

        LOG("grab key (keycode: %i, keysym: %i)\n", i, keysym);
        xcb_void_cookie_t cookie = xcb_grab_key_checked(
                                       connection,
                                       1,
                                       screen->root,
                                       0,
                                       i,
                                       XCB_GRAB_MODE_ASYNC,
                                       XCB_GRAB_MODE_ASYNC);

        if (request_failed(cookie, "cannot grab key"))
        {
            return 1;
        }

        return 0;
    }

    LOG("cannot find keycode for key (keysym: %i)\n", keysym);
    return 1;
}

int xcb_create_text_window(int pos_x, int pos_y, char *desc)
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
                                      XCB_WINDOW_WIDTH, XCB_WINDOW_HEIGHT,
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
                if (draw_text(window, 4, XCB_WINDOW_HEIGHT - 2, desc))
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

int xcb_register_for_key_event(char key)
{
    LOG("register for key event (key: %c)\n", key);
    return grab_keysym(key);
}

int xcb_wait_for_key_event(char *key)
{
    LOG("register for key event (ESC/CR)\n");
    if (grab_keysym(XK_Escape) || grab_keysym(XK_Return))
    {
        return 1;
    }

    LOG("waiting for key press event\n");
    xcb_generic_event_t *event;
    while ((event = xcb_wait_for_event(connection)))
    {
        switch (event->response_type & ~0x80)
        {
        case XCB_KEY_PRESS:
        {
            xcb_key_press_event_t *kr = (xcb_key_press_event_t *)event;

            xcb_keysym_t keysym = xcb_key_press_lookup_keysym(keysyms, kr, 0);
            LOG("key press event (keycode: %i, keysym: %i)\n", kr->detail, keysym);

            free(event);

            if (keysym == XK_Escape || keysym == XK_Return)
            {
                LOG("escape/return pressed\n");
                return 1;
            }

            *key = keysym;
            return 0;
        }
        }

        free(event);
    }

    return 1;
}

int xcb_init()
{
    connection = xcb_connect(NULL, NULL);
    if (xcb_connection_has_error(connection))
    {
        LOG("cannot open display");
        return 1;
    }

    screen = xcb_setup_roots_iterator(xcb_get_setup(connection)).data;
    keysyms = xcb_key_symbols_alloc(connection);
    return 0;
}

void xcb_finish()
{
    xcb_key_symbols_free(keysyms);
    xcb_disconnect(connection);
    connection = NULL;
    screen = NULL;
    keysyms = NULL;
}
