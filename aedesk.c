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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include "common.h"
#include "atom.h"

#define UMOD(x, y) ((((long)(x) % (long)(y)) + (y)) % (y))

Display *dpy;
Window root;
Atom net_cur_desk, net_num_desks;

static unsigned long parse_desk(char *spec);

int
main(int argc, char **argv)
{
	unsigned long desk;
	int i;

	if (argc < 2) {
		fprintf(stderr, "usage: aedesk [+-]<integer>|-n <integer>\n");
		exit(2);
	}

	dpy = XOpenDisplay(NULL);
	root = DefaultRootWindow(dpy);
	net_cur_desk = XInternAtom(dpy, "_NET_CURRENT_DESKTOP", False);
	net_num_desks = XInternAtom(dpy, "_NET_NUMBER_OF_DESKTOPS", False);

	if (!dpy) {
		fprintf(stderr, "aedesk: can't open display %s\n",
		    getenv("DISPLAY"));
		exit(1);
	}

	for (i = 1; i < argc; i++) {
		if (ARG("setn", "n", 1)) {
			desk = atol(argv[++i]);
			set_atoms(root, net_num_desks, XA_CARDINAL, &desk, 1);
		} else {
			desk = parse_desk(argv[i]);
			send_xmessage(root, root, net_cur_desk, desk,
			    SubstructureNotifyMask);
		}
	}

	XCloseDisplay(dpy);
	return 0;
}

unsigned long
parse_desk(char *spec)
{
	unsigned long ndesks, cur_desk;

	if (strchr("+-", spec[0])) {
		if (get_atoms(root, net_cur_desk, XA_CARDINAL, 0, &cur_desk, 1,
		    NULL) && get_atoms(root, net_num_desks, XA_CARDINAL, 0,
		    &ndesks, 1, NULL)) {
			return UMOD(cur_desk + atol(spec), ndesks);
		}

		return 0;
	}

	return atol(spec);
}
