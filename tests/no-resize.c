/*
 * Copyright 2020 joshua stein <jcs@jcs.org>
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

#include <stdlib.h>
#include <stdio.h>
#include <err.h>
#include <string.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include "../atom.h"
#include "../common.h"

int
main(int argc, char **argv)
{
	Window win;
	XEvent ev;
	XSizeHints *hints;
	char *title = strdup("no-resize");
	XTextProperty name;
	int screen;

	dpy = XOpenDisplay(NULL);
	if (!dpy)
		err(1, "can't open $DISPLAY");

	screen = DefaultScreen(dpy);
	root = RootWindow(dpy, screen);

	find_supported_atoms();

	win = XCreateWindow(dpy, root, 0, 0, 300, 200, 0,
	    DefaultDepth(dpy, screen), CopyFromParent,
	    DefaultVisual(dpy, screen), 0, NULL);
	if (!win)
		err(1, "XCreateWindow");

	XSetWindowBackground(dpy, win, WhitePixel(dpy, screen));

	hints = XAllocSizeHints();
	if (!hints)
		err(1, "XAllocSizeHints");

	hints->flags = PMinSize | PMaxSize;
	hints->min_width = 300;
	hints->min_height = 200;
	hints->max_width = 300;
	hints->max_height = 200;

	if (!XStringListToTextProperty(&title, 1, &name))
		err(1, "!XStringListToTextProperty");

	XSetWMProperties(dpy, win, &name, NULL, NULL, 0, hints, NULL, NULL);

	XMapWindow(dpy, win);
	while (XNextEvent(dpy, &ev))
		;

	return 0;
}
