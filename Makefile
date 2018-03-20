CC=gcc
INCS=i3ipc-glib-1.0 xcb xcb-keysyms x11
CFLAGS=$(shell pkg-config --cflags $(INCS)) --std=c99 -Wall -Wextra -D_GNU_SOURCE
LDFLAGS=$(shell pkg-config --libs $(INCS))
HEADERS=$(wildcard src/*.h)
SOURCES=$(wildcard src/*.c)
OBJECTS=$(SOURCES:.c=.o)
DEPS=$(OBJECTS:.o=.d)
EXECUTABLE=i3-easyfocus

all: $(EXECUTABLE)

debug: CFLAGS += -DDEBUG
debug: all

$(EXECUTABLE): $(OBJECTS)
	@echo "Link $@"
	@$(CC) $(OBJECTS) $(LDFLAGS) -o $@

-include $(DEPS)

.c.o:
	@echo "CC $<"
	@$(CC) -c $(CFLAGS) -MMD -o $@ $<

clean:
	@echo "Cleaning"
	@rm -f $(DEPS) $(OBJECTS) $(EXECUTABLE)
