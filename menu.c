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
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/types.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include "common.h"
#include "parser.h"
#include "atom.h"
#include "menu.h"

void
setup_switch_atoms(void)
{
	utf8_string = XInternAtom(dpy, "UTF8_STRING", False);
	wm_state = XInternAtom(dpy, "WM_STATE", False);
	net_client_list = XInternAtom(dpy, "_NET_CLIENT_LIST", False);
	net_cur_desk = XInternAtom(dpy, "_NET_CURRENT_DESKTOP", False);
	net_wm_name = XInternAtom(dpy, "_NET_WM_NAME", False);
	net_wm_desk = XInternAtom(dpy, "_NET_WM_DESKTOP", False);
	net_wm_state = XInternAtom(dpy, "_NET_WM_STATE", False);
	net_wm_state_skipt = XInternAtom(dpy, "_NET_WM_STATE_SKIP_TASKBAR",
	    False);
	net_wm_state_skipp = XInternAtom(dpy, "_NET_WM_STATE_SKIP_PAGER",
	    False);
	net_wm_wintype = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", False);
	net_wm_type_dock = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DOCK", False);
	net_wm_type_desk = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DESKTOP",
	    False);
	net_active_window = XInternAtom(dpy, "_NET_ACTIVE_WINDOW", False);
}

void
snprint_wm_name(char *buf, size_t len, Window w)
{
	char *n;

	if ((n = get_wm_name(w))) {
		if (get_wm_state(w) == NormalState) {
			if (snprintf(buf, len, "%s", n) > len)
				strcpy(buf + len - 4, "...");
		} else {
			if (snprintf(buf, len, "[%s]", n) > len)
				strcpy(buf + len - 5, "...]");
		}
		XFree(n);
	} else {
		snprintf(buf, len, "%#lx", w);
	}
}

/*
 * The WM receives the ClientMessage here and sets the property in response, so
 * we only do this after the PropertyNotify.
 */
int
is_on_cur_desk(Window w)
{
	unsigned long w_desk, cur_desk;

	if (get_atoms(root, net_cur_desk, XA_CARDINAL, 0, &cur_desk, 1, NULL) &&
	    (get_atoms(w, net_wm_desk, XA_CARDINAL, 0, &w_desk, 1, NULL)))
		return IS_ON_DESK(w_desk, cur_desk);

	return 1;
}

int
is_skip(Window w)
{
	Atom win_type, state;
	int i;
	unsigned long r;

	if (get_atoms(w, net_wm_wintype, XA_ATOM, 0, &win_type, 1, NULL) &&
	    (win_type == net_wm_type_dock || win_type == net_wm_type_desk))
		return 1;

	for (i = 0, r = 1; r; i += r)
		if ((r = get_atoms(w, net_wm_state, XA_ATOM, i, &state, 1,
		    NULL)) && (state == net_wm_state_skipt ||
		    state == net_wm_state_skipp))
			return 1;

	return 0;
}

/*
 * This XSync call is required, as we don't have any sort of integration with
 * the real X event loop.
 */
void
raise_win(Window w)
{
	XMapRaised(dpy, w);
	XSync(dpy, False);
	send_xmessage(root, w, net_active_window, 0, SubstructureNotifyMask);
}

/*
 * XXX: We do not free the command, since make_item_cb gives gives the toolkit
 * a pointer for use at some point in the future. The label we assume is copied
 * right away and thus can be freed as soon as the item is created.
 *
 * If we want to catch SIGHUP and rebuild the menu, we are going to need to
 * find some way to make sure the toolkit actually frees this stuff, or come up
 * with some fantastically ugly way of doing it ourselves.
 */
static void
do_launch_menu(FILE *rc, void *menu, make_item_func make_item_cb)
{
	FILE *f;
	char buf[BUF_SIZE], token[BUF_SIZE], *p;
	char *label;
	void *newmenu;

	while (get_rc_line(buf, sizeof buf, rc)) {
		p = buf;
		while (get_token(&p, token)) {
			if (strcmp(token, "menu") == 0) {
				if (get_token(&p, token)) {
					newmenu = make_item_cb(menu, token,
					    NULL);
					do_launch_menu(rc, newmenu,
					    make_item_cb);
				}
			}
			if (strcmp(token, "cmd") == 0) {
				if (get_token(&p, token)) {
					label = strdup(token);
					if (get_token(&p, token))
						make_item_cb(menu, label,
						    strdup(token));
					free(label);
				}
			}
			if (strcmp(token, "include") == 0) {
				if (get_token(&p, token)) {
					if ((f = fopen(token, "r"))) {
						do_launch_menu(f, menu,
						    make_item_cb);
						fclose(f);
					}
				}
			}
			if (strcmp(token, "end") == 0)
				return;
		}
	}
}

void
make_launch_menu(char *rcfile, void *menu, make_item_func make_item_cb)
{
	FILE *rc;

	if ((rc = open_rc(rcfile, "clientsrc"))) {
		do_launch_menu(rc, menu, make_item_cb);
		fclose(rc);
	} else {
		fprintf(stderr, "can't find any rc files\n");
		exit(1);
	}
}
