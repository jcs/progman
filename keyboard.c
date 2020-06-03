/*
 * Copyright (c) 2020 joshua stein <jcs@jcs.org>
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

#include "progman.h"

static client_t *cycle_head = NULL;

void
bind_keys(void)
{
	int x;

	/* Alt+Tab and Shift+Alt+Tab will cycle windows */
	XGrabKey(dpy, XKeysymToKeycode(dpy, XK_Tab), Mod1Mask, root, False,
	    GrabModeAsync, GrabModeAsync);
	XGrabKey(dpy, XKeysymToKeycode(dpy, XK_Tab), Mod1Mask | ShiftMask,
	    root, False, GrabModeAsync, GrabModeAsync);

	/* Alt+F4 closes the current window */
	XGrabKey(dpy, XKeysymToKeycode(dpy, XK_F4), Mod1Mask, root, False,
	    GrabModeAsync, GrabModeAsync);

	/* Alt+1 -> 5 switch to that desk */
	for (x = 0; x < ndesks; x++)
		XGrabKey(dpy, XKeysymToKeycode(dpy, XK_1 + x), Mod1Mask, root,
		    False, GrabModeAsync, GrabModeAsync);

	/* Super_L (Win key) launches terminal */
	XGrabKey(dpy, XKeysymToKeycode(dpy, XK_Super_L), 0, root, False,
	    GrabModeAsync, GrabModeAsync);
}

void
handle_key_event(XKeyEvent *e)
{
#ifdef DEBUG
	char buf[64];
#endif
	KeySym kc = lookup_keysym(e);
	client_t *p, *next;

#ifdef DEBUG
	snprintf(buf, sizeof(buf), "%ld [%s]", kc, e->type == KeyRelease ?
	    "up" : "down");
	dump_name(focused, __func__, buf, NULL);
#endif

	switch (kc) {
	case XK_Alt_L:
	case XK_Alt_R:
		if (e->type == KeyRelease) {
			XUngrabKeyboard(dpy, CurrentTime);
			XAllowEvents(dpy, ReplayKeyboard, e->time);
			XFlush(dpy);

			if (cycle_head) {
				cycle_head = NULL;
				if (focused && focused->state & STATE_ICONIFIED)
					uniconify_client(focused);
			}
		}
		break;
	case XK_Tab:
		if (e->state != Mod1Mask || e->type != KeyPress)
			break;

		/* keep watching input until Alt is released */
		XGrabKeyboard(dpy, root, False, GrabModeAsync, GrabModeAsync,
		    e->time);

		if (!cycle_head) {
			if (!focused)
				break;

			cycle_head = focused;
		}

		if ((next = next_client_for_focus(cycle_head)))
			focus_client(next, FOCUS_FORCE);
		else {
			/* probably at the end of the list, invert it back */
			p = focused;
			adjust_client_order(NULL, ORDER_INVERT);

			if (p)
				/* p should now not be focused */
				redraw_frame(p, None);

			focus_client(cycle_head, FOCUS_FORCE);
		}

		break;
	case XK_F4:
		if (e->state == Mod1Mask && e->type == KeyPress && focused)
			send_wm_delete(focused);
		break;
	case XK_1:
	case XK_2:
	case XK_3:
	case XK_4:
	case XK_5:
	case XK_6:
	case XK_7:
	case XK_8:
	case XK_9:
		if (e->state == Mod1Mask && e->type == KeyPress)
			goto_desk(kc - XK_1);
		break;
	case XK_Super_L:
		if (e->type == KeyPress)
			fork_exec(opt_terminal);
		break;
	}

}
