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

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <X11/Xlib.h>
#include "common.h"

Display *dpy;
Window root;

void
fork_exec(char *cmd)
{
	pid_t pid = fork();

	switch (pid) {
	case 0:
		execlp("/bin/sh", "sh", "-c", cmd, NULL);
		fprintf(stderr, "exec failed, cleaning up child\n");
		exit(1);
	case -1:
		fprintf(stderr, "can't fork\n");
	}
}

int
get_pointer(int *x, int *y)
{
	Window real_root, real_win;
	int wx, wy;
	unsigned int mask;

	XQueryPointer(dpy, root, &real_root, &real_win, x, y, &wx, &wy, &mask);
	return mask;
}

int
send_xmessage(Window w, Atom a, unsigned long x, unsigned long mask)
{
	XClientMessageEvent e;

	e.type = ClientMessage;
	e.window = w;
	e.message_type = a;
	e.format = 32;
	e.data.l[0] = x;
	e.data.l[1] = CurrentTime;

	return XSendEvent(dpy, w, False, mask, (XEvent *)&e);
}
