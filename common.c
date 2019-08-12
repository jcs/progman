/* aewm - Copyright 1998-2007 Decklin Foster <decklin@red-bean.com>. This
 * program is free software; please see LICENSE for details. */

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

	return XSendEvent(dpy, w, False, mask, (XEvent *) & e);
}
