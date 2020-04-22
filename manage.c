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

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <X11/Xatom.h>
#include "aewm.h"
#include "atom.h"

static void do_iconify(client_t *);
static void do_shade(client_t *);
static geom_t fix_size(client_t *);

static struct timespec last_close_click = { 0, 0 };

void
user_action(client_t *c, Window win, int x, int y, int button, int down)
{
	struct timespec now;
	long long tdiff;

	if (win == c->titlebar && button == 1 && down && !c->zoomed)
		move_client(c);
	else if (win == c->close && button == 1 && !down) {
		clock_gettime(CLOCK_MONOTONIC, &now);

		tdiff = (((now.tv_sec * 1000000000) + now.tv_nsec) -
		    ((last_close_click.tv_sec * 1000000000) +
		    last_close_click.tv_nsec)) / 1000000;

		if (tdiff <= DOUBLE_CLICK_MSEC)
			send_wm_delete(c);

		last_close_click.tv_sec = now.tv_sec;
		last_close_click.tv_nsec = now.tv_nsec;
	}
	else if (IS_RESIZE_WIN(c, win) && button == 1 && down && !c->shaded)
		resize_client(c, win);
	else if (((win == c->shade && button == 1) ||
	    (win == c->titlebar && button == 3)) && !down) {
		if (c->shaded)
			unshade_client(c);
		else
			shade_client(c);
	} else if (win == c->zoom && button == 1 && !down) {
		if (c->zoomed)
			unzoom_client(c);
		else
			zoom_client(c);
	}
}

Cursor
cursor_for_resize_win(client_t *c, Window win)
{
	if (win == c->resize_nw)
		return resize_nw_curs;
	else if (win == c->resize_w)
		return resize_w_curs;
	else if (win == c->resize_sw)
		return resize_sw_curs;
	else if (win == c->resize_s)
		return resize_s_curs;
	else if (win == c->resize_se)
		return resize_se_curs;
	else if (win == c->resize_e)
		return resize_e_curs;
	else if (win == c->resize_ne)
		return resize_ne_curs;
	else if (win == c->resize_n)
		return resize_n_curs;
	else
		return None;
}

/* This can't do anything dangerous. */
void
focus_client(client_t *c)
{
	client_t *oc;

	if (c) {
		set_atoms(root, net_active_window, XA_WINDOW, &c->win, 1);
		XSetInputFocus(dpy, c->win, RevertToPointerRoot, CurrentTime);
		XInstallColormap(dpy, c->cmap);
	}

	if (c != focused) {
		oc = focused;
		focused = c;
		if (c) {
			c->focus_order = ++focus_order;
			redraw_frame(c);
		}
		if (oc)
			redraw_frame(oc);
	}
}

void
move_client(client_t *c)
{
	if (c->zoomed)
		return;

	sweep(c, move_curs, recalc_move, NULL, NULL);
}

/*
 * If we are resizing a client that was zoomed, we have to put it in an
 * unzoomed state, but we need to start sweeping from the effective geometry
 * rather than the "real" geometry that unzooming will restore. We get around
 * this by blatantly cheating.
 */
void
resize_client(client_t *c, Window resize_win)
{
	strut_t hold = { 0, 0, 0, 0 };

	if (c->zoomed)
		c->save = c->geom;
	unzoom_client(c);

	sweep(c, cursor_for_resize_win(c, resize_win), recalc_resize,
	    &resize_win, &hold);

#if 0
	f = frame_geom(c);
	XMoveResizeWindow(dpy, c->frame, f.x, f.y, f.w, f.h);
	XMoveResizeWindow(dpy, c->win, BW(c), BW(c) + titlebar_height(c) + 1,
	    c->geom.w, c->geom.h);
	send_config(c);
#endif
}

/* Transients will be iconified when their owner is iconified. */
void
iconify_client(client_t *c)
{
	client_t *p;

	do_iconify(c);
	for (p = head; p; p = p->next)
		if (p->trans == c->win)
			do_iconify(p);
}

void
do_iconify(client_t *c)
{
	if (!c->ignore_unmap)
		c->ignore_unmap++;

	XUnmapWindow(dpy, c->frame);
	XUnmapWindow(dpy, c->win);
	set_wm_state(c, IconicState);
}

void
uniconify_client(client_t *c)
{
	XMapWindow(dpy, c->win);
	XMapRaised(dpy, c->frame);
	set_wm_state(c, NormalState);
}

void
shade_client(client_t *c)
{
	if (c->shaded)
		return;

	c->shaded = 1;
	append_atoms(c->win, net_wm_state, XA_ATOM, &net_wm_state_shaded, 1);
	do_shade(c);
}

void
unshade_client(client_t *c)
{
	if (!c->shaded)
		return;

	c->shaded = 0;
	remove_atom(c->win, net_wm_state, XA_ATOM, net_wm_state_shaded);
	do_shade(c);
}

static void
do_shade(client_t *c)
{
	if (c->frame) {
		recalc_frame(c);
		XMoveResizeWindow(dpy, c->frame,
		    c->frame_geom.x, c->frame_geom.y, c->frame_geom.w,
		    c->frame_geom.h);

		if (c->shaded) {
			XMoveWindow(dpy, c->win, c->geom.x, c->frame_geom.h);
			XUndefineCursor(dpy, c->resize_nw);
			XUndefineCursor(dpy, c->resize_n);
			XUndefineCursor(dpy, c->resize_ne);
			XUndefineCursor(dpy, c->resize_s);
		} else {
			XMoveWindow(dpy, c->win, c->resize_w_geom.w,
			    c->titlebar_geom.y + c->titlebar_geom.h + 1);
			XDefineCursor(dpy, c->resize_nw, resize_nw_curs);
			XDefineCursor(dpy, c->resize_n, resize_n_curs);
			XDefineCursor(dpy, c->resize_ne, resize_ne_curs);
			XDefineCursor(dpy, c->resize_s, resize_s_curs);
		}
	}
	send_config(c);
}

void
fullscreen_client(client_t *c)
{
	int screen_x = DisplayWidth(dpy, screen);
	int screen_y = DisplayHeight(dpy, screen);

	c->save = c->geom;
	c->geom.x = 0;
	c->geom.y = 0;
	c->geom.w = screen_x;
	c->geom.h = screen_y;
	c->fullscreen = 1;
	redraw_frame(c);
	send_config(c);
}

void
unfullscreen_client(client_t *c)
{
	if (!c->fullscreen)
		return;

	c->geom = c->save;
	c->fullscreen = 0;
	redraw_frame(c);
	send_config(c);
}

/*
 * When zooming a window, the old geom gets stuffed into c->save. Once we
 * unzoom, this should be considered garbage. Despite the existence of vertical
 * and horizontal hints, we only provide both at once.
 *
 * Zooming implies unshading, but the inverse is not true.
 */
void
zoom_client(client_t *c)
{
	strut_t s = { 0, 0, 0, 0 };

	if (c->zoomed)
		return;

	c->save = c->geom;
	c->shaded = 0;
	c->zoomed = 1;

	collect_struts(c, &s);
	recalc_frame(c);

	c->geom.x = s.left;
	c->geom.y = s.top;
	if (c->decor)
		c->geom.y += c->titlebar_geom.h + 1;
	c->geom.w = DisplayWidth(dpy, screen) - s.left - s.right;
	c->geom.h = DisplayHeight(dpy, screen) - s.top - s.bottom;
	if (c->decor)
		c->geom.h -= c->geom.y;

	fix_size(c);

	if (c->frame)
		redraw_frame(c);

	remove_atom(c->win, net_wm_state, XA_ATOM, net_wm_state_shaded);
	append_atoms(c->win, net_wm_state, XA_ATOM, &net_wm_state_mv, 1);
	append_atoms(c->win, net_wm_state, XA_ATOM, &net_wm_state_mh, 1);
	send_config(c);
}

void
unzoom_client(client_t *c)
{
	if (!c->zoomed)
		return;

	c->geom = c->save;
	c->zoomed = 0;

	if (c->frame) {
		recalc_frame(c);
		XMoveResizeWindow(dpy, c->frame,
		    c->frame_geom.x, c->frame_geom.y,
		    c->frame_geom.w, c->frame_geom.h);
		XResizeWindow(dpy, c->win, c->geom.w, c->geom.h);
		redraw_frame(c);
	}

	remove_atom(c->win, net_wm_state, XA_ATOM, net_wm_state_mv);
	remove_atom(c->win, net_wm_state, XA_ATOM, net_wm_state_mh);
	send_config(c);
}

/*
 * The name of this function is a little misleading: if the client doesn't
 * listen to WM_DELETE then we just terminate it with extreme prejudice.
 */
void
send_wm_delete(client_t *c)
{
	int i, n, found = 0;
	Atom *protocols;

	if (XGetWMProtocols(dpy, c->win, &protocols, &n)) {
		for (i = 0; i < n; i++)
			if (protocols[i] == wm_delete)
				found++;
		XFree(protocols);
	}
	if (found)
		send_xmessage(c->win, c->win, wm_protos, wm_delete,
		    NoEventMask);
	else
		XKillClient(dpy, c->win);
}

void
goto_desk(int new_desk)
{
	client_t *c;

	cur_desk = new_desk;
	set_atoms(root, net_cur_desk, XA_CARDINAL, &cur_desk, 1);

	for (c = head; c; c = c->next)
		map_if_desk(c);
}

void
map_if_desk(client_t *c)
{
	if (IS_ON_CUR_DESK(c) && get_wm_state(c->win) == NormalState)
		XMapWindow(dpy, c->frame);
	else
		XUnmapWindow(dpy, c->frame);
}

int
sweep(client_t *c, Cursor curs, sweep_func cb, void *cb_arg, strut_t *s)
{
	Window bounds = 0;
	XEvent ev;
	XSetWindowAttributes pattr;
	geom_t orig = c->geom, br;
	client_t *ec;
	strut_t as = { 0, 0, 0, 0 };
	int x0, y0, done = 0;

	get_pointer(&x0, &y0);
	collect_struts(c, &as);
	recalc_frame(c);

	/*
	 * Build a container window to constraint movement and resizing to
	 * prevent the top of the window from going negative x/y, and to keep
	 * the title bar on screen when moving beyond the bottom of the screen.
	 */
	br.x = as.left + x0 - orig.x + c->resize_w_geom.w;
	br.y = as.top + y0 - orig.y + c->titlebar_geom.y +
	    c->titlebar_geom.h + 1;
	br.w = DisplayWidth(dpy, screen) - as.left;
	br.h = DisplayHeight(dpy, screen) - as.top - c->titlebar_geom.h -
	    c->titlebar_geom.y;

	bounds = XCreateWindow(dpy, root, br.x, br.y, br.w, br.h, 0,
	    CopyFromParent, InputOnly, CopyFromParent, 0, &pattr);
	XMapWindow(dpy, bounds);

	if (XGrabPointer(dpy, root, False, MouseMask, GrabModeAsync,
	    GrabModeAsync, bounds, curs, CurrentTime) != GrabSuccess) {
		XDestroyWindow(dpy, bounds);
		return 0;
	}

	cb(c, orig, x0, y0, x0, y0, s, cb_arg);

	while (!done) {
		XMaskEvent(dpy, ExposureMask|MouseMask|PointerMotionMask, &ev);

		switch (ev.type) {
		case Expose:
			if ((ec = find_client(ev.xexpose.window, MATCH_FRAME)))
				redraw_frame(ec);
			break;
		case MotionNotify:
			cb(c, orig, x0, y0, ev.xmotion.x, ev.xmotion.y, s,
			    cb_arg);
			break;
		case ButtonRelease:
			done = 1;
			break;
		}
	}

	XUngrabPointer(dpy, CurrentTime);
	XDestroyWindow(dpy, bounds);

	return ev.xbutton.button;
}

/*
 * This is simple and dumb: if the cursor is in the center of the screen,
 * center the window on the available space. If it's at the top left, then at
 * the top left. As you go between, and to other edges, scale it.
 */
void
recalc_map(client_t *c, geom_t orig, int x0, int y0, int x1, int y1,
    strut_t *s, void *arg)
{
	int screen_x = DisplayWidth(dpy, screen);
	int screen_y = DisplayHeight(dpy, screen);
	int wmax = screen_x - s->left - s->right;
	int hmax = screen_y - s->top - s->bottom;

	c->geom.x = s->left + ((float) x1 / (float) screen_x) *
	    (wmax + 1 - c->geom.w - 2 * BW(c));
	c->geom.y = s->top + ((float) y1 / (float) screen_y) *
	    (hmax + 1 - c->geom.h - titlebar_height(c) - 2 * BW(c));
}

void
recalc_move(client_t *c, geom_t orig, int x0, int y0, int x1, int y1,
    strut_t *s, void *arg)
{
	c->geom.x = orig.x + x1 - x0;
	c->geom.y = orig.y + y1 - y0;

	recalc_frame(c);
	XMoveWindow(dpy, c->frame, c->frame_geom.x, c->frame_geom.y);
	send_config(c);
}

void
recalc_resize(client_t *c, geom_t orig, int x0, int y0, int x1, int y1,
    strut_t *move, void *arg)
{
	Window resize_pos = *(Window *)arg;
	geom_t now = { c->geom.x, c->geom.y, c->geom.w, c->geom.h };

	if (resize_pos == c->resize_nw)
		move->top = move->left = 1;
	else if (resize_pos == c->resize_n)
		move->top = 1;
	else if (resize_pos == c->resize_ne)
		move->top = move->right = 1;
	else if (resize_pos == c->resize_e)
		move->right = 1;
	else if (resize_pos == c->resize_se)
		move->right = move->bottom = 1;
	else if (resize_pos == c->resize_s)
		move->bottom = 1;
	else if (resize_pos == c->resize_sw)
		move->bottom = move->left = 1;
	else if (resize_pos == c->resize_w)
		move->left = 1;

	if (move->left)
		c->geom.w = orig.w + (x0 - x1);
	if (move->top)
		c->geom.h = orig.h + (y0 - y1);
	if (move->right) {
		c->geom.w = orig.w - (x0 - x1);
		c->geom.x = orig.x - (x0 - x1);
	}
	if (move->bottom) {
		c->geom.h = orig.h - (y0 - y1);
		c->geom.y = orig.y - (y0 - y1);
	}

	fix_size(c);

	if (move->left)
		c->geom.x = orig.x + orig.w - c->geom.w;
	if (move->top)
		c->geom.y = orig.y + orig.h - c->geom.h;
	if (move->right)
		c->geom.x = orig.x;
	if (move->bottom)
		c->geom.y = orig.y;

	fix_size(c);

	if (c->geom.w != now.w || c->geom.h != now.h) {
		recalc_frame(c);
		XMoveResizeWindow(dpy, c->frame,
		    c->frame_geom.x, c->frame_geom.y,
		    c->frame_geom.w, c->frame_geom.h);
		XMoveResizeWindow(dpy, c->titlebar,
		    c->frame_geom.x, c->frame_geom.y,
		    c->frame_geom.w, c->frame_geom.h);
		XResizeWindow(dpy, c->win, c->geom.w, c->geom.h);
		send_config(c);
	}
}

/*
 * If the window in question has a ResizeInc hint, then it wants to be resized
 * in multiples of some (x,y). We constrain the values in c->geom based on that
 * and any min/max size hints, and put the "human readable" values back in
 * lw_ret and lh_ret (80x25 for xterm, etc).
 */
static geom_t
fix_size(client_t *c)
{
	int width_inc, height_inc;
	int base_width, base_height;
	geom_t adj = { 0, 0, 0, 0 };

	if (c->size.flags & PMinSize) {
		if (c->geom.w < c->size.min_width)
			c->geom.w = c->size.min_width;
		if (c->geom.h < c->size.min_height)
			c->geom.h = c->size.min_height;
	}
	if (c->size.flags & PMaxSize) {
		if (c->geom.w > c->size.max_width)
			c->geom.w = c->size.max_width;
		if (c->geom.h > c->size.max_height)
			c->geom.h = c->size.max_height;
	}
	if (c->size.flags & PResizeInc) {
		width_inc = c->size.width_inc ? c->size.width_inc : 1;
		height_inc = c->size.height_inc ? c->size.height_inc : 1;
		base_width = (c->size.flags & PBaseSize) ? c->size.base_width :
		    (c->size.flags & PMinSize) ? c->size.min_width : 0;
		base_height = (c->size.flags & PBaseSize) ?
		    c->size.base_height :
		    (c->size.flags & PMinSize) ? c->size.min_height : 0;
		c->geom.w -= (c->geom.w - base_width) % width_inc;
		c->geom.h -= (c->geom.h - base_height) % height_inc;
		adj.w = (c->geom.w - base_width) / width_inc;
		adj.h = (c->geom.h - base_height) / height_inc;
	} else {
		adj.w = c->geom.w;
		adj.h = c->geom.h;
	}

	return adj;
}

#ifdef DEBUG
static const char *
show_state(client_t *c)
{
	switch (get_wm_state(c->win)) {
	SHOW(WithdrawnState)
	SHOW(NormalState)
	SHOW(IconicState)
	default:
		return "unknown state";
	}
}

static const char *
show_grav(client_t *c)
{
	if (!(c->size.flags & PWinGravity))
		return "no grav (NW)";

	switch (c->size.win_gravity) {
	SHOW(UnmapGravity)
	SHOW(NorthWestGravity)
	SHOW(NorthGravity)
	SHOW(NorthEastGravity)
	SHOW(WestGravity)
	SHOW(CenterGravity)
	SHOW(EastGravity)
	SHOW(SouthWestGravity)
	SHOW(SouthGravity)
	SHOW(SouthEastGravity)
	SHOW(StaticGravity)
	default:
		return "unknown grav";
	}
}

void
dump_name(client_t *c, const char *label, char flag)
{
	printf("%18.18s: %#010lx [%c] %-44.44s\n", label, c->win, flag,
	    c->name);
}

void
dump_win(Window w, const char *label, char flag)
{
	printf("%18.18s: %#010lx [%c] %-44.44s\n", label, w, flag,
	    w == root ? "(root window)" : "(unknown window)");
}

void
dump_info(client_t *c)
{
	printf("%31s[i] frame %#0lx, ignore_unmap %d\n", "",
	    c->frame, c->ignore_unmap);
	printf("%31s[i] desk %ld, %s, %s\n", "",
	    c->desk, show_state(c), show_grav(c));
}

void
dump_geom(client_t *c, const char *label)
{
	printf("%31s[g] %s %ldx%ld+%ld+%ld\n", "",
	    label, c->geom.w, c->geom.h, c->geom.x, c->geom.y);
}

void
dump_removal(client_t *c, int mode)
{
	printf("%31s[r] %s, %d pending\n", "",
	    mode == DEL_WITHDRAW ? "withdraw" : "remap", XPending(dpy));
}

void
dump_clients()
{
	client_t *c;

	for (c = head; c; c = c->next) {
		dump_name(c, "dump", 'd');
		dump_geom(c, "current");
		dump_info(c);
	}
}
#endif
