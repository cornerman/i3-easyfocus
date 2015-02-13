# i3-easyfocus

Focus and select windows in [i3](https://github.com/i3/i3).

Draws a small label ('a'-'z') on top of each visible container, which can be selected by pressing the corresponding key on the keyboard (cancel with ESC/CR).

## Usage

Focus the selected window:

```shell
./i3-easyfocus
```

It also possible to only print out the con_id of the selected window and, for example, move it to workspace 3:

```shell
./i3-easyfocus -p | xargs -I {} i3-msg [con_id={}] move workspace 3
```

## Dependencies

* [i3ipc-glib](https://github.com/acrisci/i3ipc-glib)
* xcb and xcb-keysyms
