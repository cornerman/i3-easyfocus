#include "xcb.h"
#include "util.h"
#include "config.h"
#include "color_config.h"

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

static uint32_t color_urgent_bg;
static uint32_t color_focused_bg;
static uint32_t color_unfocused_bg;
static uint32_t color_urgent_fg;
static uint32_t color_focused_fg;
static uint32_t color_unfocused_fg;

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
    xcb_void_cookie_t cookie = xcb_grab_key_checked(connection,
                                                    1,
                                                    screen->root,
                                                    mod_mask,
                                                    keycode,
                                                    XCB_GRAB_MODE_ASYNC,
                                                    XCB_GRAB_MODE_ASYNC);

    return request_failed(cookie, "cannot grab key");
}

static int grab_keycode(xcb_keycode_t keycode, uint16_t mod_mask)
{
    LOG("grab key (keycode: %i)\n", keycode);
    return grab_keycode_with_mod(keycode, mod_mask)                                          // key
           || grab_keycode_with_mod(keycode, mod_mask | XCB_MOD_MASK_2)                      // key with numlock
           || grab_keycode_with_mod(keycode, mod_mask | XCB_MOD_MASK_LOCK)                   // key with capslock
           || grab_keycode_with_mod(keycode, mod_mask | XCB_MOD_MASK_2 | XCB_MOD_MASK_LOCK); // key with numlock and capslock
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

static int draw_text(xcb_window_t window, int16_t x, int16_t y, uint32_t color_bg, uint32_t color_fg, const char *label)
{
    xcb_gcontext_t gc = xcb_generate_id(connection);
    uint32_t mask = XCB_GC_FOREGROUND | XCB_GC_BACKGROUND | XCB_GC_FONT;

    uint32_t value_list[3] = {color_fg, color_bg, font};

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

static int color_by_window_type(WindowType windowType, uint32_t* color_bg, uint32_t* color_fg) {
    switch(windowType)
    {
    case URGENT_WINDOW:
        *color_bg = color_urgent_bg;
        *color_fg = color_urgent_fg;
        return 0;
    case FOCUSED_WINDOW:
        *color_bg = color_focused_bg;
        *color_fg = color_focused_fg;
        return 0;
    case UNFOCUSED_WINDOW:
        *color_bg = color_unfocused_bg;
        *color_fg = color_unfocused_fg;
        return 0;
    default:
        return 1;
    }
}

int xcb_create_text_window(int pos_x, int pos_y, WindowType windowType, const char *label)
{
    uint32_t mask;
    uint32_t values[2];

    int width = predict_text_width(label);

    uint32_t color_bg, color_fg;
    if (color_by_window_type(windowType, &color_bg, &color_fg)) {
        LOG("cannot determine window colors\n");
        return 1;
    }

    xcb_window_t window = xcb_generate_id(connection);
    mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
    values[0] = color_bg;
    values[1] = XCB_EVENT_MASK_EXPOSURE;

    LOG("create window (id: %u, x: %i, y: %i): %s\n", window, pos_x, pos_y, label);
    xcb_void_cookie_t window_cookie =
        xcb_create_window_checked(connection,
                                  screen->root_depth,
                                  window,
                                  screen->root,
                                  pos_x,
                                  pos_y,
                                  width + 2,
                                  font_info->font_ascent + font_info->font_descent,
                                  0,
                                  XCB_WINDOW_CLASS_INPUT_OUTPUT,
                                  screen->root_visual,
                                  mask,
                                  values);

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

    xcb_generic_event_t *event;
    while (1)
    {
        if ((event = xcb_poll_for_event(connection)))
        {
            switch (event->response_type & ~0x80)
            {
            case XCB_EXPOSE:
            {
                free(event);
                if (draw_text(window, 1, font_info->font_ascent, color_bg, color_fg, label))
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



static uint16_t modifier_string_to_mask_fn(char *modifier, size_t size)
{
    if (strncmp(modifier, "ctrl", size) == 0)
        return XCB_MOD_MASK_CONTROL;
    else if (strncmp(modifier, "shift", size) == 0)
        return XCB_MOD_MASK_SHIFT;
    else if (strncmp(modifier, "mod1", size) == 0)
        return XCB_MOD_MASK_1;
    else if (strncmp(modifier, "mod2", size) == 0)
        return XCB_MOD_MASK_2;
    else if (strncmp(modifier, "mod3", size) == 0)
        return XCB_MOD_MASK_3;
    else if (strncmp(modifier, "mod4", size) == 0)
        return XCB_MOD_MASK_4;
    else if (strncmp(modifier, "mod5", size) == 0)
        return XCB_MOD_MASK_5;

    LOG("unknown modifier: %s. will be ignored.\n", modifier);
    return 0;
}

uint16_t xcb_modifier_string_to_mask(char *modifier)
{
    uint16_t mod_mask = 0;
    int start = 0;
    int len = strlen(modifier);
    for (int i = 0; i < len; i++)
    {
        if (modifier[i] == '+')
        {
            mod_mask |= modifier_string_to_mask_fn(modifier + start, i - start);
            start = i+1;
        }
    }

    mod_mask |= modifier_string_to_mask_fn(modifier + start, len - start);

    return mod_mask;
}

int xcb_grab_keysym(xcb_keysym_t keysym, uint16_t mod_mask)
{
    LOG("try to grab key (keysym: %i, modifier: %i)\n", keysym, mod_mask);
    xcb_keycode_t min_keycode = xcb_get_setup(connection)->min_keycode;
    xcb_keycode_t max_keycode = xcb_get_setup(connection)->max_keycode;

    xcb_keycode_t i;
    int foundCode = 0;
    for (i = min_keycode; i && i < max_keycode; i++)
    {
        if (xcb_key_symbols_get_keysym(keysyms, i, 0) != keysym)
        {
            continue;
        }

        LOG("translated keysym '%i' to keycode '%i'\n", keysym, i);
        foundCode = 1;
        if (grab_keycode(i, mod_mask))
        {
            LOG("failed to grab keycode '%i'\n", i);
            return 1;
        }
    }

    if (foundCode)
    {
        return 0;
    }

    LOG("cannot find keycode for keysym '%i' in first column\n", keysym);
    return 1;
}

xcb_keysym_t xcb_wait_for_user_input()
{
    LOG("waiting for xcb event\n");
    xcb_generic_event_t *event;
    int focused_in = 0;
    while ((event = xcb_wait_for_event(connection)))
    {
        switch (event->response_type & ~0x80)
        {
        case XCB_KEY_PRESS:
        {
            xcb_key_press_event_t *kp = (xcb_key_press_event_t *) event;

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
            LOG("focus out event\n");
            if (focused_in)
            {
                free(event);
                return XCB_NO_SYMBOL;
            }
            break;
        }
        case XCB_FOCUS_IN:
        {
            LOG("focus in event\n");
            focused_in = 1;
            break;
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

static xcb_alloc_color_cookie_t alloc_color(xcb_colormap_t colormap_id, Rgb rgb)
{
    return xcb_alloc_color(connection, colormap_id, rgb.r, rgb.g, rgb.b);
}

static int open_colors(ColorConfig cfg)
{
    xcb_colormap_t colormap_id = xcb_generate_id(connection);
    xcb_void_cookie_t open_colormap = xcb_create_colormap_checked(connection, XCB_COLORMAP_ALLOC_NONE, colormap_id, screen->root, screen->root_visual);
    if (request_failed(open_colormap, "cannot open colormap"))
    {
        return 1;
    }

    xcb_alloc_color_cookie_t urgent_bg_cookie = alloc_color(colormap_id, cfg.urgent_bg);
    xcb_alloc_color_cookie_t focused_bg_cookie = alloc_color(colormap_id, cfg.focused_bg);
    xcb_alloc_color_cookie_t unfocused_bg_cookie = alloc_color(colormap_id, cfg.unfocused_bg);
    xcb_alloc_color_cookie_t urgent_fg_cookie = alloc_color(colormap_id, cfg.urgent_fg);
    xcb_alloc_color_cookie_t focused_fg_cookie = alloc_color(colormap_id, cfg.focused_fg);
    xcb_alloc_color_cookie_t unfocused_fg_cookie = alloc_color(colormap_id, cfg.unfocused_fg);

    xcb_alloc_color_reply_t *urgent_bg_reply = xcb_alloc_color_reply(connection, urgent_bg_cookie, NULL);
    xcb_alloc_color_reply_t *focused_bg_reply = xcb_alloc_color_reply(connection, focused_bg_cookie, NULL);
    xcb_alloc_color_reply_t *unfocused_bg_reply = xcb_alloc_color_reply(connection, unfocused_bg_cookie, NULL);
    xcb_alloc_color_reply_t *urgent_fg_reply = xcb_alloc_color_reply(connection, urgent_fg_cookie, NULL);
    xcb_alloc_color_reply_t *focused_fg_reply = xcb_alloc_color_reply(connection, focused_fg_cookie, NULL);
    xcb_alloc_color_reply_t *unfocused_fg_reply = xcb_alloc_color_reply(connection, unfocused_fg_cookie, NULL);

    color_urgent_bg = urgent_bg_reply->pixel;
    color_focused_bg = focused_bg_reply->pixel;
    color_unfocused_bg = unfocused_bg_reply->pixel;
    color_urgent_fg = urgent_fg_reply->pixel;
    color_focused_fg = focused_fg_reply->pixel;
    color_unfocused_fg = unfocused_fg_reply->pixel;

    free(urgent_bg_reply);
    free(focused_bg_reply);
    free(unfocused_bg_reply);
    free(urgent_fg_reply);
    free(focused_fg_reply);
    free(unfocused_fg_reply);

    return 0;
}

static int open_font(const char *font_pattern)
{
    LOG("trying to open font: %s\n", font_pattern);
    xcb_void_cookie_t cookie = xcb_open_font_checked(connection,
                                                     font,
                                                     strlen(font_pattern),
                                                     font_pattern);

    return request_failed(cookie, "cannot open font");
}

static int open_font_with_fallback(const char *font_name)
{
    font = xcb_generate_id(connection);

    if (open_font(font_name) && open_font("-misc-*") && open_font("fixed"))
    {
        LOG("unable to find a fallback font\n");
        return 1;
    }

    return 0;
}

int xcb_init(const char *font_name, ColorConfig color_config)
{
    connection = xcb_connect(NULL, NULL);
    if (xcb_connection_has_error(connection))
    {
        LOG("cannot open display");
        return 1;
    }

    screen = xcb_setup_roots_iterator(xcb_get_setup(connection)).data;
    keysyms = xcb_key_symbols_alloc(connection);

    if (open_colors(color_config))
    {
        xcb_disconnect(connection);
        return 1;
    }

    if (open_font_with_fallback(font_name))
    {
        xcb_disconnect(connection);
        return 1;
    }

    xcb_query_font_cookie_t font_cookie = xcb_query_font(connection, font);
    font_info = xcb_query_font_reply(connection, font_cookie, NULL);

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
