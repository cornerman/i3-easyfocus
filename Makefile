all: i3-easyfocus.c
	gcc -Wall -Wextra -Werror -o i3-easyfocus -lxcb -lxcb-keysyms -lglib-2.0 -lgobject-2.0 -li3ipc-glib-1.0 -pthread -I/usr/include/json-glib-1.0 -I/usr/include/glib-2.0 -I/usr/lib/x86_64-linux-gnu/glib-2.0/include -I/usr/include/i3ipc-glib i3-easyfocus.c ipc.c xcb.c map.c win.c
