/*
 * Copyright 2020 joshua stein <jcs@jcs.org>
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
#include <string.h>
#include <unistd.h>
#include <err.h>
#include <errno.h>
#include <sys/types.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include "progman.h"

void
fork_exec(char *cmd)
{
	pid_t pid = fork();

	switch (pid) {
	case 0:
		setsid();
		execl("/bin/sh", "sh", "-c", cmd, NULL);
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
send_xmessage(Window t, Window w, Atom a, unsigned long x, unsigned long mask)
{
	XClientMessageEvent e;

	e.type = ClientMessage;
	e.window = w;
	e.message_type = a;
	e.format = 32;
	e.data.l[0] = x;
	e.data.l[1] = CurrentTime;

	return XSendEvent(dpy, t, False, mask, (XEvent *)&e);
}

action_t *
parse_action(char *prefix, char *action)
{
	char *taction = NULL, *targ = NULL;
	char *sep;
	char *sarg = NULL;
	action_t *out = NULL;
	int iaction = ACTION_NONE;
	int iarg = 0;

	taction = strdup(action);
	if ((sep = strchr(taction, ' '))) {
		*sep = '\0';
		targ = sep + 1;
	} else
		targ = NULL;

	if (strcmp(taction, "cycle") == 0)
		iaction = ACTION_CYCLE;
	else if (strcmp(taction, "reverse_cycle") == 0)
		iaction = ACTION_REVERSE_CYCLE;
	else if (strcmp(taction, "desk") == 0)
		iaction = ACTION_DESK;
	else if (strcmp(taction, "close") == 0)
		iaction = ACTION_CLOSE;
	else if (strcmp(taction, "exec") == 0)
		iaction = ACTION_EXEC;
	else if (strcmp(taction, "restart") == 0)
		iaction = ACTION_RESTART;
	else if (strcmp(taction, "quit") == 0)
		iaction = ACTION_QUIT;
	else if (taction[0] == '\n' || taction[0] == '\0')
		iaction = ACTION_NONE;
	else
		iaction = ACTION_INVALID;

	/* parse numeric or string args */
	switch (iaction) {
	case ACTION_DESK:
		if (targ == NULL) {
			warnx("%s: missing argument for \"%s\"",
			    prefix, taction);
			goto done;
		}

		if (strcmp(targ, "next") == 0)
			iaction = ACTION_DESK_NEXT;
		else if (strcmp(targ, "previous") == 0)
			iaction = ACTION_DESK_PREVIOUS;
		else {
			errno = 0;
			iarg = strtol(targ, NULL, 10);
			if (errno != 0) {
				warnx("%s: failed parsing numeric argument "
				    "\"%s\" for \"%s\"", prefix, targ, taction);
				goto done;
			}
		}
		break;
	case ACTION_EXEC:
		if (targ == NULL) {
			warnx("%s: missing string argument for \"%s\"", prefix,
			    taction);
			goto done;
		}
		sarg = strdup(targ);
		break;
	case ACTION_INVALID:
		warnx("%s: invalid action \"%s\"", prefix, taction);
		goto done;
	default:
		/* no args expected of other commands */
		if (targ != NULL) {
			warnx("%s: unexpected argument \"%s\" for \"%s\"",
			    prefix, taction, targ);
			goto done;
		}
	}

	out = malloc(sizeof(action_t));
	out->action = iaction;
	out->iarg = iarg;
	out->sarg = sarg;

done:
	if (taction)
		free(taction);

	return out;
}

void
take_action(action_t *action)
{
	client_t *p, *next;

	switch (action->action) {
	case ACTION_CYCLE:
	case ACTION_REVERSE_CYCLE:
		if (!cycle_head) {
			if (!focused)
				return;

			cycle_head = focused;
		}

		if ((next = next_client_for_focus(cycle_head)))
			focus_client(next, FOCUS_FORCE);
		else {
			/* probably at the end of the list, invert it */
			p = focused;
			adjust_client_order(NULL, ORDER_INVERT);

			if (p)
				/* p should now not be focused */
				redraw_frame(p, None);

			focus_client(cycle_head, FOCUS_FORCE);
		}
		break;
	case ACTION_DESK:
		goto_desk(action->iarg);
		break;
	case ACTION_DESK_NEXT:
		if (cur_desk < ndesks - 1)
			goto_desk(cur_desk + 1);
		break;
	case ACTION_DESK_PREVIOUS:
		if (cur_desk > 0)
			goto_desk(cur_desk - 1);
		break;
	case ACTION_CLOSE:
		if (focused)
			send_wm_delete(focused);
		break;
	case ACTION_EXEC:
		fork_exec(action->sarg);
		break;
	case ACTION_RESTART:
		cleanup();
		execlp(orig_argv0, orig_argv0, NULL);
		break;
	case ACTION_QUIT:
		quit();
		break;
	default:
		warnx("unhandled action %d\n", action->action);
	}
}
