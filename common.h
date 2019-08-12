/* aewm - Copyright 1998-2007 Decklin Foster <decklin@red-bean.com>. This
 * program is free software; please see LICENSE for details. */

#ifndef AEWM_COMMON_H
#define AEWM_COMMON_H

#include <X11/Xlib.h>

#define SYS_RC_DIR "/etc/X11/aewm"

/* Hooray for magic numbers */

#define DESK_ALL 0xFFFFFFFF
#define IS_ON_DESK(w, d) (w == d || w == DESK_ALL)

#define BUF_SIZE 2048
#define ARG(longname, shortname, nargs) \
    ((strcmp(argv[i], "--" longname) == 0 || \
    strcmp(argv[i], "-" shortname) == 0) && i + (nargs) < argc)

extern Display *dpy;
extern Window root;

extern void fork_exec(char *);
extern int get_pointer(int *, int *);
extern int send_xmessage(Window, Atom, unsigned long, unsigned long);

#endif	/* AEWM_COMMON_H */
