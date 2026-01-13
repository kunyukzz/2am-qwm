## 2am-qwm – quiet window manager

Minimal X11 window manager written in C using xcb.
Built from a learning-based project and now used daily.
No DBus, no IPC, no systemd assumptions.
Works on old and low-end hardware.
Init-system agnostic (systemd, runit, openrc), tested on Void & Arch Linux.
Built-in taskbar with kernel-based status.
Built-in launcher to run your applications.
5 workspaces, tiling / monocle / floating layouts.
If it runs X, it can run qwm.

### Building

Configuration is done in source code.
Build: `cc build.c -o build && ./build`
or you can create your own Makefile
