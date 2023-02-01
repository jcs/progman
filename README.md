## progman

progman is a simple X11 window manager modeled after Program Manager from
the Windows 3 era.

![progman screenshot](https://jcs.org/images/progman-20200810.png)

### License

MIT

### Compiling

Run `make` to compile, and `make install` to install to `/usr/local` by
default.

### Features

- Window minimizing, drawing icons and labels on the root/desktop
- Window maximizing and full-screen support (via `_NET_WM_STATE_FULLSCREEN`)
- Window shading by double-clicking on a window titlebar
- Window moving by holding down `Alt` and clicking anywhere on a window
- Built-in keyboard binding support by adding items to the `[keyboard]`
  section of `~/.config/progman/progman.ini` such as `Win+L = exec xlock`
- Built-in mouse button binding on the desktop by adding items to the
  `[desktop]` section of `~/.config/progman/progman.ini` such as
  `Mouse3 = exec xterm`, with right-click setup by default to show a
  configurable launcher menu containing programs listed in the `[launcher]`
  section of `progman.ini`
- Virtual desktops with keyboard shortcuts for switching between them bound
  to `Alt+1` through `Alt+0` by default, and using the mouse wheel on the
  desktop to scroll through virtual desktops
- Window cycling with `Alt+Tab` and `Shift+Alt+Tab`
- [Theme support](https://github.com/jcs/progman/tree/master/themes)
- Optional HiDPI support by defining `-DHIDPI` in `Makefile`, to double-size
  images, icons, and borders
