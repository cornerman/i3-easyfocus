FROM debian

RUN apt-get update -y

RUN apt-get install -y autoconf xcb-proto libxcb-keysyms1-dev glibc-source libjson-glib-dev gtk-doc-tools gobject-introspection libtool libx11-dev git

RUN git clone https://github.com/altdesktop/i3ipc-glib/ \
    && cd i3ipc-glib \
    && ./autogen.sh \
    && make \
    && make install

RUN git clone https://github.com/cornerman/i3-easyfocus \
    && cd i3-easyfocus \
    && make \
    && mv i3-easyfocus /usr/local/bin
