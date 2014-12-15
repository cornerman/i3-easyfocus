all: i3-easyfocus.c
	gcc -Wall -Wextra -Werror -o i3-easyfocus $(shell pkg-config --cflags --libs glib-2.0 gobject-2.0 i3ipc-glib-1.0 xcb xcb-keysyms) i3-easyfocus.c ipc.c xcb.c map.c win.c
