## quiet window manager

Minimal X11 window manager written in C using xcb. Built from a learning-based project and now used daily.

- No DBus, no IPC, no systemd assumptions
- Works on old and low-end hardware
- Init-system agnostic, tested on Void & Arch Linux
- Self-contained, Built-in taskbar and launcher 
- 5 workspaces, tiling / monocle / floating layouts

If it runs X, it can run qwm.

### Important Notes

- Uses raw XCB keycodes: assumes US QWERTY keyboard layout
- Single monitor only (dual monitor untested) - that's what i have
- Configuration requires editing source and recompiling

### Building

Configuration is done in source code.
Build: `cc build.c -o build && ./build`
or you can create your own Makefile
