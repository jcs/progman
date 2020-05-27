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
	/* Alt+Tab and Shift+Alt+Tab */
	XGrabKey(dpy, XKeysymToKeycode(dpy, XK_Tab), Mod1Mask, root, False,
	    GrabModeAsync, GrabModeAsync);
	XGrabKey(dpy, XKeysymToKeycode(dpy, XK_Tab), Mod1Mask | ShiftMask,
	    root, False, GrabModeAsync, GrabModeAsync);

#if 0
	XGrabKey(dpy, XKeysymToKeycode(dpy, XK_Alt_L), 0, root, False,
	    GrabModeAsync, GrabModeAsync);
	XGrabKey(dpy, XKeysymToKeycode(dpy, XK_Alt_R), 0, root, False,
	    GrabModeAsync, GrabModeAsync);
#endif
}

KeySym
lookup_keysym(XKeyEvent *e)
{
	KeySym keysym;
	XLookupString(e, NULL, 512, &keysym, NULL);
	return keysym;
}

void
handle_key_event(XKeyEvent *e)
{
#ifdef DEBUG
	char buf[64];
#endif
	KeySym kc = lookup_keysym(e);
	client_t *p;

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

		if (!cycle_head)
			cycle_head = focused;

		if (cycle_head) {
			if (cycle_head->next)
				focus_client(cycle_head->next,
				    FOCUS_FORCE);
			else {
				/* circled around */
				p = focused;
				adjust_client_order(NULL, ORDER_INVERT);
				/* redraw as unfocused */
				redraw_frame(p, None);
				focus_client(cycle_head, FOCUS_FORCE);
				break;
			}

		}

		break;
	}

}
