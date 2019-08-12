/* aewm - Copyright 1998-2007 Decklin Foster <decklin@red-bean.com>. This
 * program is free software; please see LICENSE for details. */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <X11/Xatom.h>
#include "aewm.h"
#include "atom.h"

static void do_iconify(client_t *);
static void do_shade(client_t *);
static void draw_outline(client_t *);
static geom_t fix_size(client_t *);

void
user_action(client_t *c, int x, int y, int button)
{
	if (x >= c->geom.w - frame_height(c) && y <= frame_height(c)) {
		switch (button) {
		case Button1:iconify_client(c);
			break;
		case Button2:
			resize_client(c);
			break;
		case Button3:
			send_wm_delete(c);
			break;
		case Button4:
			zoom_client(c);
			break;
		case Button5:
			unzoom_client(c);
			break;
		}
	} else {
		switch (button) {
		case Button1:
			XRaiseWindow(dpy, c->frame);
			break;
		case Button2:
			move_client(c);
			break;
		case Button3:
			XLowerWindow(dpy, c->frame);
			break;
		case Button4:
			shade_client(c);
			break;
		case Button5:
			unshade_client(c);
			break;
		}
	}
}

/* This can't do anything dangerous. See handle_enter_event. */
void
focus_client(client_t *c)
{
	set_atoms(root, net_active_window, XA_WINDOW, &c->win, 1);
	XSetInputFocus(dpy, c->win, RevertToPointerRoot, CurrentTime);
	XInstallColormap(dpy, c->cmap);
}

void
move_client(client_t *c)
{
	geom_t f;

	if (!c->zoomed) {
		sweep(c, move_curs, recalc_move, SWEEP_UP, NULL);
		f = frame_geom(c);
		XMoveWindow(dpy, c->frame, f.x, f.y);
		send_config(c);
	}
}

/*
 * If we are resizing a client that was zoomed, we have to put it in an
 * unzoomed state, but we need to start sweeping from the effective geometry
 * rather than the "real" geometry that unzooming will restore. We get around
 * this by blatantly cheating.
 */
void
resize_client(client_t *c)
{
	geom_t f;
	strut_t hold = {0, 0, 0, 0};

	if (c->zoomed)
		c->save = c->geom;
	unzoom_client(c);

	sweep(c, resize_curs, recalc_resize, SWEEP_UP, &hold);
	f = frame_geom(c);
	XMoveResizeWindow(dpy, c->frame, f.x, f.y, f.w, f.h);
	XMoveResizeWindow(dpy, c->win, 0, frame_height(c), c->geom.w,
	    c->geom.h);
	send_config(c);
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
	if (!c->shaded) {
		c->shaded = 1;
		append_atoms(c->win, net_wm_state, XA_ATOM,
		    &net_wm_state_shaded, 1);
		do_shade(c);
	}
}

void
unshade_client(client_t *c)
{
	if (c->shaded) {
		c->shaded = 0;
		remove_atom(c->win, net_wm_state, XA_ATOM, net_wm_state_shaded);
		do_shade(c);
	}
}

static void
do_shade(client_t *c)
{
	geom_t f;

	if (c->frame) {
		f = frame_geom(c);
		XMoveResizeWindow(dpy, c->frame, f.x, f.y, f.w, f.h);
	}
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
	strut_t s = {0, 0, 0, 0};

	if (!c->zoomed) {
		c->save = c->geom;
		c->shaded = 0;
		c->zoomed = 1;

		collect_struts(c, &s);
		c->geom.x = s.left;
		c->geom.y = s.top;
		c->geom.w = DisplayWidth(dpy, screen) - 2 * BW(c) - s.left -
		    s.right;
		c->geom.h = DisplayHeight(dpy, screen) - 2 * BW(c) -
		    frame_height(c) - s.top - s.bottom;
		fix_size(c);

		if (c->frame) {
			XMoveResizeWindow(dpy, c->frame, c->geom.x, c->geom.y,
			    c->geom.w, c->geom.h + frame_height(c));
			XResizeWindow(dpy, c->win, c->geom.w, c->geom.h);
			redraw_frame(c);
		}
		remove_atom(c->win, net_wm_state, XA_ATOM, net_wm_state_shaded);
		append_atoms(c->win, net_wm_state, XA_ATOM, &net_wm_state_mv,
		    1);
		append_atoms(c->win, net_wm_state, XA_ATOM, &net_wm_state_mh,
		    1);
		send_config(c);
	}
}

void
unzoom_client(client_t *c)
{
	geom_t f;

	if (c->zoomed) {
		c->geom = c->save;
		c->zoomed = 0;

		if (c->frame) {
			f = frame_geom(c);
			XMoveResizeWindow(dpy, c->frame, f.x, f.y, f.w, f.h);
			XResizeWindow(dpy, c->win, c->geom.w, c->geom.h);
			redraw_frame(c);
		}
		remove_atom(c->win, net_wm_state, XA_ATOM, net_wm_state_mv);
		remove_atom(c->win, net_wm_state, XA_ATOM, net_wm_state_mh);
		send_config(c);
	}
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
		send_xmessage(c->win, wm_protos, wm_delete, NoEventMask);
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
sweep(client_t *c, Cursor curs, sweep_func cb, int mode, strut_t *s)
{
	int x0, y0, mask;
	geom_t orig = c->geom;
	XEvent ev;

	if (XGrabPointer(dpy, root, False, MouseMask, GrabModeAsync,
		GrabModeAsync, None, curs, CurrentTime) != GrabSuccess)
		return 0;

	XGrabServer(dpy);

	mask = get_pointer(&x0, &y0);
	cb(c, orig, x0, y0, x0, y0, s);
	draw_outline(c);

	/* XXX: Ugh */
	if (!(mode == SWEEP_DOWN && (mask & ButtonPressMask))) {
		for (;;) {
			XMaskEvent(dpy, MouseMask, &ev);
			if (ev.type == MotionNotify) {
				draw_outline(c);
				cb(c, orig, x0, y0, ev.xmotion.x, ev.xmotion.y,
				    s);
				draw_outline(c);
			} else if ((mode == SWEEP_UP &&
			    ev.type == ButtonRelease) ||
			    (mode == SWEEP_DOWN && ev.type == ButtonPress)) {
				draw_outline(c);
				break;
			}
		}
	}
	XUngrabServer(dpy);
	XUngrabPointer(dpy, CurrentTime);

	return ev.xbutton.button;
}

/*
 * This is simple and dumb: if the cursor is in the center of the screen,
 * center the window on the available space. If it's at the top left, then at
 * the top left. As you go between, and to other edges, scale it.
 */
void
recalc_map(client_t *c, geom_t orig, int x0, int y0, int x1, int y1, strut_t *s)
{
	int screen_x = DisplayWidth(dpy, screen);
	int screen_y = DisplayHeight(dpy, screen);
	int wmax = screen_x - s->left - s->right;
	int hmax = screen_y - s->top - s->bottom;

	c->geom.x = s->left + ((float) x1 / (float) screen_x) *
	    (wmax + 1 - c->geom.w - 2 * BW(c));
	c->geom.y = s->top + ((float) y1 / (float) screen_y) *
	    (hmax + 1 - c->geom.h - frame_height(c) - 2 * BW(c));
}

void
recalc_move(client_t *c, geom_t orig, int x0, int y0, int x1, int y1,
    strut_t *s)
{
	c->geom.x = orig.x + x1 - x0;
	c->geom.y = orig.y + y1 - y0;
}

/*
 * When considering the distance from the mouse to the center point of the
 * window (which remains fixed), we actually have to look at the top right
 * corner of the window (which remains a constant offset from wherever we
 * clicked in the box relative to the root, but not relative to the window,
 * because the distance can be negative). After that we just center the new
 * size.
 */
void
recalc_resize(client_t *c, geom_t orig, int x0, int y0, int x1, int y1,
    strut_t *hold)
{
	client_t fake = *c;	/* FIXME */
	geom_t frig;

	fake.geom = orig;
	frig = frame_geom(&fake);

	if (x1 <= frig.x)
		hold->left = 1;
	if (y1 <= frig.y)
		hold->top = 1;
	if (x1 >= frig.x + frig.w)
		hold->right = 1;
	if (y1 >= frig.y + frig.h)
		hold->bottom = 1;

	if (hold->left)
		c->geom.w = orig.x + orig.w - x1;
	if (hold->top)
		c->geom.h = orig.y + orig.h - y1;
	if (hold->right)
		c->geom.w = x1 - orig.x;
	if (hold->bottom)
		c->geom.h = y1 - orig.y;

	if (x1 > frig.x + frig.w)
		hold->left = 0;
	if (y1 > frig.y + frig.h)
		hold->top = 0;
	if (x1 < frig.x)
		hold->right = 0;
	if (y1 < frig.y)
		hold->bottom = 0;

	fix_size(c);
	if (hold->left)
		c->geom.x = orig.x + orig.w - c->geom.w;
	if (hold->top)
		c->geom.y = orig.y + orig.h - c->geom.h;
	if (hold->right)
		c->geom.x = orig.x;
	if (hold->bottom)
		c->geom.y = orig.y;
}

/*
 * If the window in question has a ResizeInc hint, then it wants to be resized
 * in multiples of some (x,y). We constrain the values in c->geom based on that
 * and any min/max size hints, and put the ``human readable'' values back in
 * lw_ret and lh_ret (80x25 for xterm, etc).
 */
static geom_t
fix_size(client_t *c)
{
	int width_inc, height_inc;
	int base_width, base_height;
	geom_t adj = {0, 0, 0, 0};

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

/*
 * Match the calculations we use to draw the frames, and also the spacing of
 * text from the opposite corner.
 */
static void
draw_outline(client_t *c)
{
	geom_t adj, f = frame_geom(c);
	int re = f.x + f.w + BW(c);
	int be = f.y + f.h + BW(c);
	char buf[256];

	XDrawRectangle(dpy, root, invert_gc, f.x + BW(c) / 2, f.y + BW(c) / 2,
	    f.w + BW(c), f.h + BW(c));
	if (!c->shaded)
		XDrawLine(dpy, root, invert_gc, f.x + BW(c),
		    f.y + frame_height(c) + BW(c) / 2, re, f.y +
		    frame_height(c) + BW(c) / 2);
	XDrawLine(dpy, root, invert_gc, re - frame_height(c) + BW(c) / 2,
	    f.y + BW(c), re - frame_height(c) + BW(c) / 2, f.y +
	    frame_height(c));

	adj = fix_size(c);
	snprintf(buf, sizeof buf, "%ldx%ld%+ld%+ld", adj.w, adj.h, c->geom.x,
	    c->geom.y);
	XDrawString(dpy, root, invert_gc,
	    re - opt_pad - font->descent / 2 - XTextWidth(font, buf,
	    strlen(buf)), be - opt_pad - font->descent, buf, strlen(buf));
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
	printf("%15.15s: %#010lx [%c] %-47.47s\n", label, c->win, flag,
	    c->name);
}

void
dump_win(Window w, const char *label, char flag)
{
	printf("%15.15s: %#010lx [%c] %-47.47s\n", label, w, flag,
	    w == root ? "(root window)" : "(unknown window)");
}

void
dump_info(client_t *c)
{
	printf("%28s[i] frame %#0lx, ignore_unmap %d\n", "",
	    c->frame, c->ignore_unmap);
	printf("%28s[i] desk %ld, %s, %s\n", "",
	    c->desk, show_state(c), show_grav(c));
}

void
dump_geom(client_t *c, const char *label)
{
	printf("%28s[g] %s %ldx%ld+%ld+%ld\n", "",
	    label, c->geom.w, c->geom.h, c->geom.x, c->geom.y);
}

void
dump_removal(client_t *c, int mode)
{
	printf("%28s[r] %s, %d pending\n", "",
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
