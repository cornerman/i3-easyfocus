#ifndef I3_EASYFOCUS_WIN
#define I3_EASYFOCUS_WIN

typedef struct window
{
    struct window *next;
    int id;
    struct
    {
        int x;
        int y;
    } position;
} window;

window *window_append(window *win, window *item);
void window_free(window *win);

#endif
