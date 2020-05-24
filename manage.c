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
#include <X11/extensions/shape.h>
#include "progman.h"
#include "atom.h"

static void do_iconify(client_t *);
static void do_shade(client_t *);
static void maybe_toolbar_click(client_t *, Window);
static void monitor_toolbar_click(client_t *, geom_t, int, int, int, int,
    strut_t *, void *);

static struct {
	struct timespec tv;
	client_t *c;
	Window win;
	int button;
} last_click = { { 0, 0 }, NULL, 0 };

void
user_action(client_t *c, Window win, int x, int y, int button, int down)
{
	struct timespec now;
	long long tdiff;
	int double_click = 0;

	if (!down) {
		clock_gettime(CLOCK_MONOTONIC, &now);

		if (last_click.button == button && c == last_click.c &&
		    win == last_click.win) {
			tdiff = (((now.tv_sec * 1000000000) + now.tv_nsec) -
			    ((last_click.tv.tv_sec * 1000000000) +
			    last_click.tv.tv_nsec)) / 1000000;
			if (tdiff <= DOUBLE_CLICK_MSEC)
				double_click = 1;
		}

		last_click.button = button;
		last_click.c = c;
		last_click.win = win;
		memcpy(&last_click.tv, &now, sizeof(now));
	}

#ifdef DEBUG
	printf("%s(\"%s\", %lx, %d, %d, %d, %d) double:%d, c state %d\n",
	    __func__, c->name, win, x, y, button, down, double_click, c->state);
	dump_info(c);
#endif

	if (c->state & STATE_ICONIFIED &&
	    (win == c->icon || win == c->icon_label)) {
	    	if (down && !double_click) {
			move_client(c);
			get_pointer(&x, &y);
			user_action(c, win, x, y, button, 0);
		} else if (!down && double_click)
			uniconify_client(c);
	} else if (win == c->titlebar) {
		if (button == 1 && down) {
			if (!(c->state & STATE_ZOOMED)) {
				move_client(c);
				/* sweep() eats the ButtonRelease event */
				get_pointer(&x, &y);
				user_action(c, win, x, y, button, 0);
			}
		} else if (button == 1 && !down && double_click) {
			if (c->state & STATE_ZOOMED)
				unzoom_client(c);
			else
				zoom_client(c);
		} else if (button == 3 && !down) {
			if (c->state & STATE_SHADED)
				unshade_client(c);
			else
				shade_client(c);
		}
	} else if (win == c->close) {
		if (button == 1 && down) {
			maybe_toolbar_click(c, win);
			if (!c->close_pressed)
				return;

			c->close_pressed = False;
			redraw_frame(c, c->close);

			get_pointer(&x, &y);
			user_action(c, win, x, y, button, 0);
		}

		if (double_click)
			send_wm_delete(c);
	} else if (IS_RESIZE_WIN(c, win)) {
		if (button == 1 && down && !(c->state & STATE_SHADED))
			resize_client(c, win);
	} else if (win == c->iconify) {
		if (button == 1 && down) {
			maybe_toolbar_click(c, win);
			if (c->iconify_pressed) {
				c->iconify_pressed = False;
				redraw_frame(c, c->iconify);
				if (c->state & STATE_ICONIFIED)
					uniconify_client(c);
				else
					iconify_client(c);
			}
		}
	} else if (win == c->zoom) {
		if (button == 1 && down) {
			maybe_toolbar_click(c, win);
			if (c->zoom_pressed) {
				c->zoom_pressed = False;
				redraw_frame(c, c->zoom);
				if (c->state & STATE_ZOOMED)
					unzoom_client(c);
				else
					zoom_client(c);
			}
		}
	}

	if (double_click)
		/* don't let a 3rd click keep counting as a double click */
		last_click.tv.tv_sec = last_click.tv.tv_nsec = 0;
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

	if (c && (c->state & STATE_ICONIFIED)) {
		set_atoms(root, net_active_window, XA_WINDOW, &c->icon, 1);
		XSetInputFocus(dpy, c->icon, RevertToPointerRoot, CurrentTime);
	} else if (c) {
		set_atoms(root, net_active_window, XA_WINDOW, &c->win, 1);
		XSetInputFocus(dpy, c->win, RevertToPointerRoot, CurrentTime);
		XInstallColormap(dpy, c->cmap);
	}

	if (c != focused) {
		oc = focused;
		focused = c;
		if (c) {
			c->focus_order = ++focus_order;
			redraw_frame(c, c->titlebar);
		}
		if (oc)
			redraw_frame(oc, oc->titlebar);
	}
}

void
move_client(client_t *c)
{
	strut_t s = { 0 };

	if (c->state & (STATE_ZOOMED | STATE_FULLSCREEN | STATE_DOCK))
		return;

	collect_struts(c, &s);
	sweep(c, move_curs, recalc_move, NULL, &s);

	send_config(c);

	if (c->state & STATE_ICONIFIED)
		redraw_icon(c, None);
	else
		redraw_frame(c, None);

	flush_expose_client(c);
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

	if (c->state & STATE_ZOOMED) {
		c->save = c->geom;
		unzoom_client(c);
	}

	sweep(c, cursor_for_resize_win(c, resize_win), recalc_resize,
	    &resize_win, &hold);
}

/*
 * The user has clicked on a toolbar button but may mouse off of it and then
 * let go, so only consider it a click if the mouse is still there when the
 * mouse button is released.
 */
void
maybe_toolbar_click(client_t *c, Window win)
{
	if (win == c->iconify)
		c->iconify_pressed = True;
	else if (win == c->zoom)
		c->zoom_pressed = True;
	else if (win == c->close)
		c->close_pressed = True;
	else
		return;

	redraw_frame(c, win);
	sweep(c, None, monitor_toolbar_click, &win, NULL);
	redraw_frame(c, win);
}

void
monitor_toolbar_click(client_t *c, geom_t orig, int x0, int y0, int x1, int y1,
    strut_t *s, void *arg)
{
	Window win = *(Window *)arg;
	geom_t *geom;
	Bool was, *pr;

	if (win == c->iconify) {
		geom = &c->iconify_geom;
		pr = &c->iconify_pressed;
	} else if (win == c->zoom) {
		geom = &c->zoom_geom;
		pr = &c->zoom_pressed;
	} else if (win == c->close) {
		geom = &c->close_geom;
		pr = &c->close_pressed;
	} else
		return;

	was = *pr;

	if (x1 >= (c->frame_geom.x + geom->x) &&
	    x1 <= (c->frame_geom.x + geom->x + geom->w) &&
	    y1 >= (c->frame_geom.y + geom->y) &&
	    y1 <= (c->frame_geom.y + geom->y + geom->h))
		*pr = True;
	else
		*pr = False;

	if (was != *pr)
		redraw_frame(c, win);
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

	focus_client(prev_focused());
}

void
do_iconify(client_t *c)
{
	XSetWindowAttributes attrs = { 0 };
	strut_t s = { 0 };
	XGCValues gv;

	if (!c->ignore_unmap)
		c->ignore_unmap++;
	XUnmapWindow(dpy, c->frame);
	XUnmapWindow(dpy, c->win);
	c->state |= STATE_ICONIFIED;
	set_wm_state(c, IconicState);

	get_client_icon(c);

	if (c->icon_name)
		XFree(c->icon_name);
	c->icon_name = get_wm_icon_name(c->win);

	/* TODO: if not ICON_SIZE, scale up/down */
	if (c->icon_geom.w < 1)
		c->icon_geom.w = ICON_SIZE;
	if (c->icon_geom.h < 1)
		c->icon_geom.h = ICON_SIZE;

	attrs.background_pixel = BlackPixel(dpy, screen);
	attrs.event_mask = ButtonPressMask | ButtonReleaseMask |
	    VisibilityChangeMask | ExposureMask | KeyPressMask |
	    EnterWindowMask | FocusChangeMask;

	collect_struts(c, &s);
	/* TODO: find a suitable spot that won't overlap any other icons */
	c->icon_geom.x = s.left + ICON_SIZE;
	c->icon_geom.y = DisplayHeight(dpy, screen) - s.top - s.bottom -
	    c->icon_geom.h - (ICON_SIZE * 2);

	c->icon = XCreateWindow(dpy, root, c->icon_geom.x, c->icon_geom.h,
	    c->icon_geom.w, c->icon_geom.h, 0, CopyFromParent, CopyFromParent,
	    CopyFromParent, CWBackPixel | CWEventMask, &attrs);
	XMapWindow(dpy, c->icon);

	c->icon_label = XCreateWindow(dpy, root, 0, 0, c->icon_geom.w,
	    c->icon_geom.h, 0, CopyFromParent, CopyFromParent, CopyFromParent,
	    CWBackPixel | CWEventMask, &attrs);
	XMapWindow(dpy, c->icon_label);
	c->icon_xftdraw = XftDrawCreate(dpy, (Drawable)c->icon_label,
	    DefaultVisual(dpy, screen), DefaultColormap(dpy, screen));

	c->icon_gc = XCreateGC(dpy, c->icon, 0, &gv);

	redraw_icon(c, None);
}

void
uniconify_client(client_t *c)
{
	XMapWindow(dpy, c->win);
	XMapRaised(dpy, c->frame);
	c->state &= ~STATE_ICONIFIED;
	set_wm_state(c, NormalState);

	c->ignore_unmap++;
	XDestroyWindow(dpy, c->icon);
	c->icon = None;
	c->ignore_unmap++;
	XDestroyWindow(dpy, c->icon_label);
	c->icon_label = None;

	redraw_frame(c, None);
	focus_client(c);
}

void
shade_client(client_t *c)
{
	if (c->state != STATE_NORMAL || !c->decor)
		return;

	c->state |= STATE_SHADED;
	append_atoms(c->win, net_wm_state, XA_ATOM, &net_wm_state_shaded, 1);
	do_shade(c);
}

void
unshade_client(client_t *c)
{
	if (!(c->state & STATE_SHADED))
		return;

	c->state &= ~(STATE_SHADED);
	remove_atom(c->win, net_wm_state, XA_ATOM, net_wm_state_shaded);
	do_shade(c);
}

static void
do_shade(client_t *c)
{
	if (c->frame) {
		redraw_frame(c, None);

		if (c->state & STATE_SHADED) {
			XUndefineCursor(dpy, c->resize_nw);
			XUndefineCursor(dpy, c->resize_n);
			XUndefineCursor(dpy, c->resize_ne);
			XUndefineCursor(dpy, c->resize_s);
		} else {
			XDefineCursor(dpy, c->resize_nw, resize_nw_curs);
			XDefineCursor(dpy, c->resize_n, resize_n_curs);
			XDefineCursor(dpy, c->resize_ne, resize_ne_curs);
			XDefineCursor(dpy, c->resize_s, resize_s_curs);
		}
	}
	send_config(c);
	flush_expose_client(c);
}

void
fullscreen_client(client_t *c)
{
	int screen_x = DisplayWidth(dpy, screen);
	int screen_y = DisplayHeight(dpy, screen);

#ifdef DEBUG
	dump_name(c, __func__, NULL, c->name);
#endif

	if (c->state & (STATE_FULLSCREEN | STATE_DOCK))
		return;

	if (c->state & STATE_SHADED)
		unshade_client(c);

	c->save = c->geom;
	c->geom.x = 0;
	c->geom.y = 0;
	c->geom.w = screen_x;
	c->geom.h = screen_y;
	c->state |= STATE_FULLSCREEN;
	redraw_frame(c, None);
	send_config(c);
	flush_expose_client(c);
}

void
unfullscreen_client(client_t *c)
{
#ifdef DEBUG
	dump_name(c, __func__, NULL, c->name);
#endif

	if (!(c->state & STATE_FULLSCREEN))
		return;

	c->geom = c->save;
	c->state &= ~STATE_FULLSCREEN;

	redraw_frame(c, None);
	send_config(c);
	flush_expose_client(c);
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
	strut_t s = { 0 };

	if (c->state & STATE_DOCK)
		return;

	if (c->state & STATE_SHADED)
		unshade_client(c);

	c->save = c->geom;
	c->state |= STATE_ZOOMED;

	collect_struts(c, &s);
	recalc_frame(c);

	c->geom.x = s.left;
	c->geom.y = s.top + c->titlebar_geom.h + (c->titlebar_geom.h ? 1 : 0);
	c->geom.w = DisplayWidth(dpy, screen) - s.left - s.right;
	c->geom.h = DisplayHeight(dpy, screen) - s.top - s.bottom - c->geom.y;

	redraw_frame(c, None);

	append_atoms(c->win, net_wm_state, XA_ATOM, &net_wm_state_mv, 1);
	append_atoms(c->win, net_wm_state, XA_ATOM, &net_wm_state_mh, 1);
	send_config(c);
	flush_expose_client(c);
}

void
unzoom_client(client_t *c)
{
	if (!(c->state & STATE_ZOOMED))
		return;

	c->geom = c->save;
	c->state &= ~STATE_ZOOMED;
	redraw_frame(c, None);

	remove_atom(c->win, net_wm_state, XA_ATOM, net_wm_state_mv);
	remove_atom(c->win, net_wm_state, XA_ATOM, net_wm_state_mh);
	send_config(c);
	flush_expose_client(c);
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

static XEvent sweepev;
void
sweep(client_t *c, Cursor curs, sweep_func cb, void *cb_arg, strut_t *s)
{
	geom_t orig = (c->state & STATE_ICONIFIED ? c->icon_geom : c->geom);
	client_t *ec;
	strut_t as = { 0 };
	int x0, y0, done = 0;

	get_pointer(&x0, &y0);
	collect_struts(c, &as);
	recalc_frame(c);

	if (XGrabPointer(dpy, root, False, MouseMask, GrabModeAsync,
	    GrabModeAsync, root, curs, CurrentTime) != GrabSuccess)
		return;

	cb(c, orig, x0, y0, x0, y0, s, cb_arg);

	while (!done) {
		XMaskEvent(dpy, ExposureMask | MouseMask | PointerMotionMask |
		    StructureNotifyMask | SubstructureNotifyMask, &sweepev);
#ifdef DEBUG
		show_event(sweepev);
#endif
		switch (sweepev.type) {
		case Expose:
			if ((ec = find_client(sweepev.xexpose.window,
			    MATCH_FRAME)))
				redraw_frame(ec, sweepev.xexpose.window);
			break;
		case MotionNotify:
			cb(c, orig, x0, y0, sweepev.xmotion.x,
			    sweepev.xmotion.y, s, cb_arg);
			break;
		case ButtonRelease:
			done = 1;
			break;
		case UnmapNotify:
		case DestroyNotify:
			/* this may affect our window, better stop */
			done = 1;
			XPutBackEvent(dpy, &sweepev);
			break;
		}
	}

	XUngrabPointer(dpy, CurrentTime);
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
	    (hmax + 1 - c->geom.h - TITLEBAR_HEIGHT(c) - 2 * BW(c));
}

void
recalc_move(client_t *c, geom_t orig, int x0, int y0, int x1, int y1,
    strut_t *s, void *arg)
{
	int newx = orig.x + x1 - x0;
	int newy = orig.y + y1 - y0;
	int sw = DisplayWidth(dpy, screen);
	int sh = DisplayHeight(dpy, screen);
	geom_t tg;

	if (c->state & STATE_ICONIFIED) {
		int xd = newx - c->icon_geom.x;
		int yd = newy - c->icon_geom.y;

		c->icon_geom.x = newx;
		c->icon_geom.y = newy;
		c->icon_label_geom.x += xd;
		c->icon_label_geom.y += yd;

		XMoveWindow(dpy, c->icon, c->icon_geom.x, c->icon_geom.y);
		XMoveWindow(dpy, c->icon_label, c->icon_label_geom.x,
		    c->icon_label_geom.y);
		send_config(c);
		flush_expose_client(c);
		return;
	}

	sw -= s->right;
	sh -= s->bottom;
	memcpy(&tg, &c->frame_geom, sizeof(c->frame_geom));

	/* provide some resistance at screen edges */
	if (x1 < x0) {
		/* left edge */
		if (newx - c->resize_w_geom.w >= (long)s->left ||
		    newx - c->resize_w_geom.w < (long)s->left -
		    opt_edge_resist)
			c->geom.x = newx;
	} else {
		/* right edge */
		if (newx + c->geom.w + c->resize_e_geom.w <= sw ||
		    newx + c->geom.w + c->resize_e_geom.w > sw +
		    opt_edge_resist)
			c->geom.x = newx;
	}

	if (y1 < y0) {
		/* top edge */
		if (newy - c->resize_n_geom.h - c->titlebar_geom.h >=
		    (long)s->top ||
		    newy - c->resize_n_geom.h - c->titlebar_geom.h <
		    (long)s->top - opt_edge_resist)
			c->geom.y = newy;
	} else {
		/* bottom edge */
		if (newy + c->geom.h + c->resize_s_geom.h <= sh ||
		    newy + c->geom.h + c->resize_s_geom.h > sh +
		    opt_edge_resist)
			c->geom.y = newy;
	}

	recalc_frame(c);

	if (c->frame_geom.x == tg.x && c->frame_geom.y == tg.y)
		return;

	XMoveWindow(dpy, c->frame, c->frame_geom.x, c->frame_geom.y);
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
		redraw_frame(c, None);
		send_config(c);
	}
}

/*
 * If the window in question has a ResizeInc hint, then it wants to be resized
 * in multiples of some (x,y). We constrain the values in c->geom based on that
 * and any min/max size hints.
 */
void
fix_size(client_t *c)
{
	int width_inc, height_inc;
	int base_width, base_height;

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
	}
}

/* make sure a frame fits on the screen */
void
constrain_frame(client_t *c)
{
	strut_t s = { 0 };
	geom_t *g;
	int h, w;

	if (c->state != STATE_NORMAL)
		return;

#ifdef DEBUG
	dump_geom(c, c->geom, "constrain_frame initial");
#endif

	recalc_frame(c);

	collect_struts(c, &s);

	if (c->decor)
		g = &c->frame_geom;
	else
		g = &c->geom;

	if (g->x < s.top)
		g->x = s.top;
	if (g->y < s.left)
		g->y = s.left;

	h = DisplayHeight(dpy, screen) - s.top - s.bottom;
	if (g->y + g->h > h)
		g->h = h - g->y;

	w = DisplayWidth(dpy, screen) - s.left - s.right;
	if (g->x + g->w > w)
		g->w = w - g->x;

	if (c->decor) {
		/*
		 * recalc_frame adjusts based on c->geom, and we've been
		 * changing c->frame_geom, so shrink c->geom to the frame_geom
		 */
		c->geom.w = c->frame_geom.w - c->resize_w_geom.w -
		    c->resize_e_geom.w;
		c->geom.h = c->frame_geom.h - c->resize_nw_geom.h - 1 -
		    c->resize_s_geom.h;
	}

	fix_size(c);
	recalc_frame(c);
#ifdef DEBUG
	dump_geom(c, c->geom, "constrain_frame final");
#endif
}

void
flush_expose_client(client_t *c)
{
	if (c->resize_nw)
		flush_expose(c->resize_nw);
	if (c->resize_n)
		flush_expose(c->resize_n);
	if (c->resize_ne)
		flush_expose(c->resize_ne);
	if (c->resize_e)
		flush_expose(c->resize_e);
	if (c->resize_se)
		flush_expose(c->resize_se);
	if (c->resize_s)
		flush_expose(c->resize_s);
	if (c->resize_sw)
		flush_expose(c->resize_sw);
	if (c->resize_w)
		flush_expose(c->resize_w);
	if (c->close)
		flush_expose(c->close);
	if (c->iconify)
		flush_expose(c->iconify);
	if (c->zoom)
		flush_expose(c->zoom);
	if (c->titlebar)
		flush_expose(c->titlebar);
	if (c->icon)
		flush_expose(c->icon);
	if (c->icon_label)
		flush_expose(c->icon_label);
}

/* remove expose events for a window from the event queue */
void
flush_expose(Window win)
{
	XEvent junk;
	while (XCheckTypedWindowEvent(dpy, win, Expose, &junk))
		;
}

#ifdef DEBUG
char *
state_name(client_t *c)
{
	int s = 30;
	char *res = malloc(s);
	res[0] = '\0';

	if (c->state == STATE_NORMAL)
		strlcat(res, "normal", s);
	if (c->state & STATE_ZOOMED)
		strlcat(res, "| zoomed", s);
	if (c->state & STATE_ICONIFIED)
		strlcat(res, "| iconified", s);
	if (c->state & STATE_SHADED)
		strlcat(res, "| shaded", s);
	if (c->state & STATE_FULLSCREEN)
		strlcat(res, "| fs", s);
	if (c->state & STATE_DOCK)
		strlcat(res, "| dock", s);

	if (res[0] == '|')
		res = strdup(res + 2);

	return res;
}

const char *
frame_name(client_t *c, Window w)
{
	if (w == None)
		return "";
	if (w == c->frame)
		return "frame";
	if (w == c->resize_nw)
		return "resize_nw";
	if (w == c->resize_w)
		return "resize_w";
	if (w == c->resize_sw)
		return "resize_sw";
	if (w == c->resize_s)
		return "resize_s";
	if (w == c->resize_se)
		return "resize_se";
	if (w == c->resize_e)
		return "resize_e";
	if (w == c->resize_ne)
		return "resize_ne";
	if (w == c->resize_n)
		return "resize_n";
	if (w == c->titlebar)
		return "titlebar";
	if (w == c->close)
		return "close";
	if (w == c->iconify)
		return "iconify";
	if (w == c->zoom)
		return "zoom";
	if (w == c->icon)
		return "icon";
	if (w == c->icon_label)
		return "icon_label";
	return "unknown";
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
dump_name(client_t *c, const char *label, const char *detail, const char *name)
{
	printf("%18.18s: %#010lx [%-9.9s] %-35.35s\n", label,
	    c ? c->win : 0, detail == NULL ? "" : detail,
	    name == NULL ? "" : name);
}

void
dump_info(client_t *c)
{
	char *s = state_name(c);

	printf("%31s[i] decor %d, ignore_unmap %d, trans 0x%lx\n", "",
	    c->decor, c->ignore_unmap, c->trans);
	printf("%31s[i] desk %ld, state %s, %s\n", "",
	    c->desk, s, show_grav(c));

	free(s);
}

void
dump_geom(client_t *c, geom_t g, const char *label)
{
	printf("%31s[g] %s %ldx%ld+%ld+%ld\n", "",
	    label, g.w, g.h, g.x, g.y);
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
		dump_name(c, __func__, NULL, c->name);
		dump_geom(c, c->geom, "current");
		dump_info(c);
	}
}
#endif
