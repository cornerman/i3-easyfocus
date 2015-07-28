#include "xcb.h"
#include "util.h"
#include "config.h"

#include <stdlib.h>
#include <string.h>
#include <xcb/xcb_keysyms.h>
#include <X11/Xlib.h>

#define BUFFER 512

static xcb_connection_t *connection = NULL;
static xcb_screen_t *screen = NULL;
static xcb_key_symbols_t *keysyms = NULL;
static xcb_font_t font;
static xcb_query_font_reply_t *font_info = NULL;

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

static int grab_keycode_with_mod(xcb_keycode_t keycode, uint16_t mod_mask)
{
    xcb_void_cookie_t cookie = xcb_grab_key_checked(
                                   connection,
                                   1,
                                   screen->root,
                                   mod_mask,
                                   keycode,
                                   XCB_GRAB_MODE_ASYNC,
                                   XCB_GRAB_MODE_ASYNC);

    return request_failed(cookie, "cannot grab key");
}

static int grab_keycode(xcb_keycode_t keycode)
{
    LOG("grab key (keycode: %i)\n", keycode);
    return grab_keycode_with_mod(keycode, 0) // key
           || grab_keycode_with_mod(keycode, XCB_MOD_MASK_2) // key with numlock
           || grab_keycode_with_mod(keycode, XCB_MOD_MASK_LOCK) // key with capslock
           || grab_keycode_with_mod(keycode, XCB_MOD_MASK_2 | XCB_MOD_MASK_LOCK); // key with numlock and capslock
}

static xcb_char2b_t *string_to_char2b(const char *text, size_t len)
{
    xcb_char2b_t *str = malloc(sizeof(xcb_char2b_t) * len);
    size_t i;
    for (i = 0; i < len; i++)
    {
        str[i].byte1 = '\0';
        str[i].byte2 = text[i];
    }

    return str;
}

static int predict_text_width(const char *text)
{
    size_t len = strlen(text);
    xcb_char2b_t *str = string_to_char2b(text, len);

    xcb_query_text_extents_cookie_t cookie = xcb_query_text_extents(connection, font, len, str);
    xcb_query_text_extents_reply_t *reply = xcb_query_text_extents_reply(connection, cookie, NULL);
    if (reply == NULL)
    {
        fprintf(stderr, "cannot predict text width for '%s'\n", text);
        free(str);
        return font_info->max_bounds.character_width * len;
    }

    int width = reply->overall_width;

    free(str);
    free(reply);
    return width;
}

static int draw_text(xcb_window_t window, int16_t x, int16_t y, const char *label)
{
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

int xcb_create_text_window(int pos_x, int pos_y, const char *label)
{
    uint32_t mask;
    uint32_t values[2];

    int width = predict_text_width(label);

    xcb_window_t window = xcb_generate_id(connection);
    mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
    values[0] = screen->white_pixel;
    values[1] = XCB_EVENT_MASK_EXPOSURE;

    LOG("create window (id: %i, x: %i, y: %i): %s\n", window, pos_x, pos_y, label);
    xcb_void_cookie_t window_cookie = xcb_create_window_checked(connection,
                                      screen->root_depth,
                                      window, screen->root,
                                      pos_x, pos_y,
                                      width + 2,
                                      font_info->font_ascent + font_info->font_descent,
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
                if (draw_text(window, 1, font_info->font_ascent, label))
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
}

char *xcb_keysym_to_string(xcb_keysym_t keysym)
{
    //TODO: this is not really the correct string representation of the key
    char *key = XKeysymToString(keysym);
    if (key == NULL)
    {
        return NULL;
    }

    char *key_buf = malloc(BUFFER);
    strncpy(key_buf, key, BUFFER - 1);
    key_buf[BUFFER - 1] = '\0';

    return key_buf;
}

int xcb_grab_keysym(xcb_keysym_t keysym)
{
    LOG("try to grab key (keysym: %i)\n", keysym);
    xcb_keycode_t min_keycode = xcb_get_setup(connection)->min_keycode;
    xcb_keycode_t max_keycode = xcb_get_setup(connection)->max_keycode;

    xcb_keycode_t i;
    for (i = min_keycode; i && i < max_keycode; i++)
    {
        if (xcb_key_symbols_get_keysym(keysyms, i, 0) != keysym)
        {
            continue;
        }

        LOG("translated keysym '%i' to keycode '%i'\n", keysym, i);
        return grab_keycode(i);
    }

    LOG("cannot find keycode for keysym '%i' in first column\n", keysym);
    return 1;
}

xcb_keysym_t xcb_wait_for_user_input()
{
    LOG("waiting for xcb event\n");
    xcb_generic_event_t *event;
    while ((event = xcb_wait_for_event(connection)))
    {
        switch (event->response_type & ~0x80)
        {
        case XCB_KEY_PRESS:
        {
            xcb_key_press_event_t *kp = (xcb_key_press_event_t *)event;

            xcb_keysym_t sym = xcb_key_press_lookup_keysym(keysyms, kp, 0);
            LOG("key press event (keycode: %i, keysym: %i)\n", kp->detail, sym);

            free(event);
            return sym;
        }
        case XCB_CONFIGURE_NOTIFY:
        {
            LOG("configure notify event\n");

            free(event);
            return XCB_NO_SYMBOL;
        }
        case XCB_FOCUS_OUT:
        {
            LOG("focus change event\n");

            free(event);
            return XCB_NO_SYMBOL;
        }
        }

        free(event);
    }

    return 1;
}

int xcb_register_configure_notify()
{
    LOG("registering for configure notify event\n");
    uint32_t mask = XCB_CW_EVENT_MASK;
    uint32_t values[1];
    values[0] = XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY | XCB_EVENT_MASK_FOCUS_CHANGE;
    xcb_void_cookie_t cookie = xcb_change_window_attributes_checked(connection,
                               screen->root,
                               mask,
                               values);

    if (request_failed(cookie, "cannot register for events on root window"))
    {
        return 1;
    }

    xcb_flush(connection);
    return 0;
}

int xcb_init()
{
    connection = xcb_connect(NULL, NULL);
    if (xcb_connection_has_error(connection))
    {
        LOG("cannot open display");
        return 1;
    }

    font = xcb_generate_id(connection);
    xcb_void_cookie_t cookie = xcb_open_font_checked(connection,
                               font,
                               strlen(XCB_FONT_NAME),
                               XCB_FONT_NAME);

    if (request_failed(cookie, "cannot open font"))
    {
        xcb_disconnect(connection);
        return 1;
    }

    xcb_query_font_cookie_t font_cookie = xcb_query_font(connection, font);
    font_info = xcb_query_font_reply(connection, font_cookie, NULL);

    screen = xcb_setup_roots_iterator(xcb_get_setup(connection)).data;
    keysyms = xcb_key_symbols_alloc(connection);

    return 0;
}

void xcb_finish()
{
    free(font_info);
    xcb_close_font(connection, font);
    xcb_key_symbols_free(keysyms);
    xcb_disconnect(connection);
    connection = NULL;
}
