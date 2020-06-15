/*
 * Copyright 1998-2007 Decklin Foster <decklin@red-bean.com>.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef PROGMAN_COMMON_H
#define PROGMAN_COMMON_H

#include <X11/Xlib.h>

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
extern int send_xmessage(Window, Window, Atom, unsigned long, unsigned long);
extern void bind_key(char *, char *);

#endif	/* PROGMAN_COMMON_H */
