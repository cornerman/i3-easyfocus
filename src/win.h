#ifndef I3_EASYFOCUS_WIN
#define I3_EASYFOCUS_WIN

typedef struct window
{
    struct window *next;
    int id;
    int win_id;
    struct
    {
        int x;
        int y;
    } position;
} Window;

Window *window_append(Window *win, Window *item);
void window_free(Window *win);

#endif
