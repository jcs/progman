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
#ifdef DEBUG
#include <stdio.h>
#endif
#include <string.h>
#include <X11/Xatom.h>
#include <X11/extensions/shape.h>
#include "progman.h"
#include "atom.h"

static void do_map(client_t *, int);
static void init_geom(client_t *, strut_t *);
static void reparent(client_t *, strut_t *);
static void bevel(Window, int);

/*
 * Set up a client structure for the new (not-yet-mapped) window. We have to
 * ignore two unmap events if the client was already mapped but has IconicState
 * set (for instance, when we are the second window manager in a session).
 * That's because there's one for the reparent (which happens on all viewable
 * windows) and then another for the unmapping itself.
 */
client_t *
new_client(Window w)
{
	client_t *c;
	XWindowAttributes attr;
	XColor exact;
	long supplied;
	Atom win_type;

	c = malloc(sizeof *c);
	memset(c, 0, sizeof(*c));
	c->next = head;
	head = c;

	c->name = get_wm_name(w);
	c->win = w;
	c->frame = None;
	c->resize_nw = None;
	c->resize_n = None;
	c->resize_ne = None;
	c->resize_e = None;
	c->resize_se = None;
	c->resize_s = None;
	c->resize_sw = None;
	c->resize_w = None;
	c->titlebar = None;
	c->close = None;
	c->iconify = None;
	c->zoom = None;
	c->decor = 1;

	XGetWMNormalHints(dpy, c->win, &c->size, &supplied);
	XGetTransientForHint(dpy, c->win, &c->trans);

	XGetWindowAttributes(dpy, c->win, &attr);
	c->geom.x = attr.x;
	c->geom.y = attr.y;
	c->geom.w = attr.width;
	c->geom.h = attr.height;
	c->cmap = attr.colormap;
	c->old_bw = attr.border_width;

#ifdef DEBUG
	dump_name(c, "creating", 'w');
	dump_geom(c, "initial");
#endif

	XAllocNamedColor(dpy, c->cmap, opt_fg, &fg, &exact);
	XAllocNamedColor(dpy, c->cmap, opt_bg, &bg, &exact);
	XAllocNamedColor(dpy, c->cmap, opt_fg_unfocused, &fg_unfocused, &exact);
	XAllocNamedColor(dpy, c->cmap, opt_bg_unfocused, &bg_unfocused, &exact);
	XAllocNamedColor(dpy, c->cmap, opt_bd, &bd, &exact);

	if (get_atoms(c->win, net_wm_wintype, XA_ATOM, 0, &win_type, 1, NULL)) {
		c->decor = HAS_DECOR(win_type);
		c->dock = (win_type == net_wm_type_dock);
	}

	if (get_atoms(c->win, net_wm_desk, XA_CARDINAL, 0, &c->desk, 1, NULL)) {
		if (c->desk == -1)
			c->desk = DESK_ALL;	/* FIXME */
		if (c->desk >= ndesks && c->desk != DESK_ALL)
			c->desk = cur_desk;
	} else {
		set_atoms(c->win, net_wm_desk, XA_CARDINAL, &cur_desk, 1);
		c->desk = cur_desk;
	}
#ifdef DEBUG
	dump_info(c);
#endif

	check_states(c);

	/*
	 * We are not actually keeping the stack one in order. However, every
	 * fancy panel uses it and nothing else, no matter what the spec says.
	 * (I'm not sure why, as rearranging the list every time the stacking
	 * changes would be distracting. GNOME's window list applet
	 * doesn't.)
	 */
	append_atoms(root, net_client_list, XA_WINDOW, &c->win, 1);
	append_atoms(root, net_client_stack, XA_WINDOW, &c->win, 1);

	XGrabButton(dpy, Button1, Mod1Mask, c->win, True,
	    ButtonPressMask | ButtonReleaseMask, GrabModeAsync, GrabModeAsync,
	    None, move_curs);

	return c;
}

client_t *
find_client(Window w, int mode)
{
	client_t *c;

	for (c = head; c; c = c->next) {
		switch (mode) {
		case MATCH_ANY:
			if (w == c->frame || w == c->win || w == c->resize_nw ||
			    w == c->resize_w || w == c->resize_sw ||
			    w == c->resize_s || w == c->resize_se ||
			    w == c->resize_e || w == c->resize_ne ||
			    w == c->resize_n || w == c->titlebar ||
			    w == c->close || w == c->iconify || w == c->zoom)
				return c;
			break;
		case MATCH_FRAME:
			if (w == c->frame || w == c->resize_nw ||
			    w == c->resize_w || w == c->resize_sw ||
			    w == c->resize_s || w == c->resize_se ||
			    w == c->resize_e || w == c->resize_ne ||
			    w == c->resize_n || w == c->titlebar ||
			    w == c->close || w == c->iconify || w == c->zoom)
				return c;
			break;
		case MATCH_WINDOW:
			if (w == c->win)
				return c;
		}
	}

	return NULL;
}

client_t *
top_client(void)
{
	unsigned int nwins, i;
	Window qroot, qparent, *wins;
	XWindowAttributes attr;
	client_t *c;

	XQueryTree(dpy, root, &qroot, &qparent, &wins, &nwins);
	for (i = nwins - 1; i > 0; i--) {
		XGetWindowAttributes(dpy, wins[i], &attr);
		if ((c = find_client(wins[i], MATCH_FRAME)))
			return c;
	}
	XFree(wins);

	return NULL;
}

client_t *
prev_focused(void)
{
	client_t *c = head;
	client_t *prev = NULL;
	unsigned int high = 0;

	while (c) {
		if (c->focus_order > high && !c->dock && c != focused) {
			high = c->focus_order;
			prev = c;
		}
		c = c->next;
	}

	return prev;
}

void
map_client(client_t *c)
{
	XWindowAttributes attr;
	strut_t s = {0, 0, 0, 0};
	XWMHints *hints;
	int want_raise = !c->dock;

	XGrabServer(dpy);

	XGetWindowAttributes(dpy, c->win, &attr);
	collect_struts(c, &s);

	if (attr.map_state == IsViewable) {
		c->ignore_unmap++;
		reparent(c, &s);
		init_geom(c, &s);

		if (get_wm_state(c->win) == IconicState) {
			c->ignore_unmap++;
			XUnmapWindow(dpy, c->win);
		} else {
			set_wm_state(c, NormalState);
			do_map(c, want_raise);
		}
	} else {
		if ((hints = XGetWMHints(dpy, c->win))) {
			if (hints->flags & StateHint)
				set_wm_state(c, hints->initial_state);
			else
				set_wm_state(c, NormalState);
			XFree(hints);
		} else {
			set_wm_state(c, NormalState);
		}
		init_geom(c, &s);
#ifdef DEBUG
		dump_geom(c, "set to");
		dump_info(c);
#endif
		reparent(c, &s);
		if (get_wm_state(c->win) == NormalState)
			do_map(c, want_raise);
	}

	XSync(dpy, False);
	c->name = get_wm_name(c->win);
	/* horrible kludge */
	XUngrabServer(dpy);
}

/*
 * This is just a helper to perform the actual mapping, since there are two
 * different places we might need to do it.
 */
static void
do_map(client_t *c, int do_raise)
{
	if (!IS_ON_CUR_DESK(c))
		return;

	if (do_raise) {
		XMapRaised(dpy, c->frame);
		if (c->resize_nw)
			XMapRaised(dpy, c->resize_nw);
		if (c->resize_n)
			XMapRaised(dpy, c->resize_n);
		if (c->resize_ne)
			XMapRaised(dpy, c->resize_ne);
		if (c->resize_e)
			XMapRaised(dpy, c->resize_e);
		if (c->resize_se)
			XMapRaised(dpy, c->resize_se);
		if (c->resize_s)
			XMapRaised(dpy, c->resize_s);
		if (c->resize_sw)
			XMapRaised(dpy, c->resize_sw);
		if (c->resize_w)
			XMapRaised(dpy, c->resize_w);
		if (c->close)
			XMapRaised(dpy, c->close);
		if (c->iconify)
			XMapRaised(dpy, c->iconify);
		if (c->zoom)
			XMapRaised(dpy, c->zoom);
		if (c->titlebar)
			XMapRaised(dpy, c->titlebar);
		XMapRaised(dpy, c->win);
		focus_client(c);
	} else {
		XLowerWindow(dpy, c->frame);
		XMapWindow(dpy, c->frame);
		if (c->resize_nw)
			XMapWindow(dpy, c->resize_nw);
		if (c->resize_n)
			XMapWindow(dpy, c->resize_n);
		if (c->resize_ne)
			XMapWindow(dpy, c->resize_ne);
		if (c->resize_e)
			XMapWindow(dpy, c->resize_e);
		if (c->resize_se)
			XMapWindow(dpy, c->resize_se);
		if (c->resize_s)
			XMapWindow(dpy, c->resize_s);
		if (c->resize_sw)
			XMapWindow(dpy, c->resize_sw);
		if (c->resize_w)
			XMapWindow(dpy, c->resize_w);
		if (c->close)
			XMapWindow(dpy, c->close);
		if (c->iconify)
			XMapWindow(dpy, c->iconify);
		if (c->zoom)
			XMapWindow(dpy, c->zoom);
		if (c->titlebar)
			XMapWindow(dpy, c->titlebar);
		XMapWindow(dpy, c->win);
	}
}

/*
 * When we're ready to map, we have two things to consider: the literal
 * geometry of the window (what the client passed to XCreateWindow), and the
 * size hints (what they set with XSetWMSizeHints, if anything). Generally, the
 * client doesn't care, and leaves the literal geometry at +0+0. If the client
 * wants to be mapped in a particular place, though, they either set this
 * geometry to something different or set a size hint. The size hint is the
 * recommended method, and takes precedence. If there is already something in
 * c->geom, though, we just leave it.
 */
static void
init_geom(client_t *c, strut_t *s)
{
	Atom win_type, state;
	int screen_x = DisplayWidth(dpy, screen);
	int screen_y = DisplayHeight(dpy, screen);
	int wmax = screen_x - s->left - s->right;
	int hmax = screen_y - s->top - s->bottom;
	int mouse_x, mouse_y;

	/*
	 * We decide the geometry for these types of windows, so we can just
	 * ignore everything and return right away. If c->zoomed is set, that
	 * means we've already set things up, but otherwise, we do it here.
	 */
	if (c->zoomed)
		return;

	if (get_atoms(c->win, net_wm_state, XA_ATOM, 0, &state, 1, NULL) &&
	    state == net_wm_state_fs) {
		fullscreen_client(c);
		return;
	}

	/*
	 * Here, we merely set the values; they're in the same place regardless
	 * of whether the user or the program specified them. We'll distinguish
	 * between the two cases later, if we need to.
	 */
	if (c->size.flags & (USSize | PSize)) {
		if (c->size.width > 0)
			c->geom.w = c->size.width;
		if (c->size.height > 0)
			c->geom.h = c->size.height;
	}

	if (c->size.flags & (USPosition | PPosition)) {
		if (c->size.x > 0)
			c->geom.x = c->size.x;
		if (c->size.y > 0)
			c->geom.y = c->size.y;
	}

	/*
	 * Several types of windows can put themselves wherever they want, but
	 * we need to read the size hints to get that position before
	 * returning.
	 */
	if (get_atoms(c->win, net_wm_wintype, XA_ATOM, 0, &win_type, 1, NULL) &&
	    CAN_PLACE_SELF(win_type))
		return;

	/*
	 * At this point, maybe nothing was set, or something went horribly
	 * wrong and the values are garbage. So, make a guess, based on the
	 * pointer.
	 */
	if (!c->placed && c->geom.x <= 0 && c->geom.y <= 0) {
		get_pointer(&mouse_x, &mouse_y);
		recalc_map(c, c->geom, mouse_x, mouse_y, mouse_x, mouse_y, s,
		    NULL);
	}

	/*
	 * In any case, if we got this far, we need to do a further sanity
	 * check and make sure that the window isn't overlapping any struts --
	 * except for transients, because they might be a panel-type client
	 * popping up a notification window over themselves.
	 */
	if (!c->trans) {
		if (c->geom.x + c->geom.w > screen_x - s->right)
			c->geom.x = screen_x - s->right - c->geom.w;
		if (c->geom.y + c->geom.h > screen_y - s->bottom)
			c->geom.y = screen_y - s->bottom - c->geom.h;
		if (c->geom.x < s->left || c->geom.w > wmax)
			c->geom.x = s->left;
		if (c->geom.y < s->top || c->geom.h > hmax)
			c->geom.y = s->top;
	}

	/*
	 * And now, wherever the client thought it was going, that's where the
	 * frame is going, so adjust the client accordingly.
	 */
	if (c->decor) {
		recalc_frame(c);

		/* only move already-placed windows if they're off-screen */
		if (!c->placed || (c->frame_geom.x < 0 || c->geom.y <= 0)) {
			c->geom.x += (c->geom.x - c->frame_geom.x);
			c->geom.y += (c->geom.y - c->frame_geom.y);
			recalc_frame(c);
		}
	}
}

/*
 * The frame window is not created until we actually do the reparenting here,
 * and thus the Xft surface cannot exist until this runs. Anything that has to
 * manipulate the client before we are called must make sure not to attempt to
 * use either.
 */
static void
reparent(client_t *c, strut_t *s)
{
	XSetWindowAttributes pattr;

	recalc_frame(c);

	pattr.override_redirect = True;
	pattr.background_pixel = bd.pixel;
	pattr.event_mask = SubMask | ButtonPressMask | ButtonReleaseMask |
	    ExposureMask | EnterWindowMask;
	c->frame = XCreateWindow(dpy, root,
	    c->frame_geom.x, c->frame_geom.y,
	    c->frame_geom.w, c->frame_geom.h,
	    0,
	    DefaultDepth(dpy, screen), CopyFromParent,
	    DefaultVisual(dpy, screen),
	    CWOverrideRedirect | CWBackPixel | CWEventMask, &pattr);

	XSetWindowBackground(dpy, c->frame, BlackPixel(dpy, screen));

	if (c->decor) {
		/*
		 * These all get changed to button_bg.pixel in redraw_frame,
		 * but make them black to start with so a slow-drawing window
		 * just has a solid black background.
		 */
		pattr.background_pixel = BlackPixel(dpy, screen);

#define _(x,y,z) x##y##z
#define CREATE_RESIZE_WIN(DIR) \
		pattr.cursor = _(resize_,DIR,_curs); \
		_(c->resize_,DIR,) = XCreateWindow(dpy, c->frame, \
		    _(c->resize_,DIR,_geom.x), _(c->resize_,DIR,_geom.y), \
		    _(c->resize_,DIR,_geom.w), _(c->resize_,DIR,_geom.h), \
		    0, CopyFromParent, InputOutput, CopyFromParent, \
		    CWOverrideRedirect | CWBackPixel | CWEventMask | CWCursor, \
		    &pattr);

		CREATE_RESIZE_WIN(nw);
		CREATE_RESIZE_WIN(n);
		CREATE_RESIZE_WIN(ne);
		CREATE_RESIZE_WIN(e);
		CREATE_RESIZE_WIN(se);
		CREATE_RESIZE_WIN(s);
		CREATE_RESIZE_WIN(sw);
		CREATE_RESIZE_WIN(w);
#undef _
#undef CREATE_RESIZE_WIN

		/* no CWCursor for these */
		c->close = XCreateWindow(dpy, c->frame,
		    c->close_geom.x, c->close_geom.y,
		    c->close_geom.w, c->close_geom.h,
		    0, CopyFromParent, InputOutput, CopyFromParent,
		    CWOverrideRedirect | CWBackPixel | CWEventMask, &pattr);

		c->titlebar = XCreateWindow(dpy, c->frame,
		    c->titlebar_geom.x, c->titlebar_geom.y,
		    c->titlebar_geom.w, c->titlebar_geom.h,
		    0, CopyFromParent, InputOutput, CopyFromParent,
		    CWOverrideRedirect | CWBackPixel | CWEventMask, &pattr);

		c->iconify = XCreateWindow(dpy, c->frame,
		    c->iconify_geom.x, c->iconify_geom.y,
		    c->iconify_geom.w, c->iconify_geom.h,
		    0, CopyFromParent, InputOutput, CopyFromParent,
		    CWOverrideRedirect | CWBackPixel | CWEventMask, &pattr);

		c->zoom = XCreateWindow(dpy, c->frame,
		    c->zoom_geom.x, c->zoom_geom.y,
		    c->zoom_geom.w, c->zoom_geom.h,
		    0, CopyFromParent, InputOutput, CopyFromParent,
		    CWOverrideRedirect | CWBackPixel | CWEventMask, &pattr);

		c->xftdraw = XftDrawCreate(dpy, (Drawable)c->titlebar,
		    DefaultVisual(dpy, DefaultScreen(dpy)),
		    DefaultColormap(dpy, DefaultScreen(dpy)));
	}

	if (shape) {
		XShapeSelectInput(dpy, c->win, ShapeNotifyMask);
		set_shape(c);
	}

	XAddToSaveSet(dpy, c->win);
	XSelectInput(dpy, c->win, ColormapChangeMask | PropertyChangeMask);
	XSetWindowBorderWidth(dpy, c->win, 0);
	XResizeWindow(dpy, c->win, c->geom.w, c->geom.h);
	XReparentWindow(dpy, c->win, c->frame, c->resize_w_geom.w,
	    c->titlebar_geom.y + c->titlebar_geom.h + 1);

	send_config(c);
}

/* TODO: replace callers with macro */
int
titlebar_height(client_t *c)
{
	return TITLEBAR_HEIGHT(c);
}

void
recalc_frame(client_t *c)
{
	int th = titlebar_height(c);
	int bw = BW(c);

	if (!c->decor) {
		c->frame_geom.x = c->geom.x;
		c->frame_geom.y = c->geom.y;
		c->frame_geom.w = c->geom.w;
		c->frame_geom.h = c->geom.h;
		return;
	}

	if (c->zoomed)
		bw = 1;

	c->resize_nw_geom.x = 0;
	c->resize_nw_geom.y = 0;
	c->resize_nw_geom.w = bw + th;
	c->resize_nw_geom.h = bw + th;

	c->close_geom.x = bw;
	c->close_geom.y = bw;
	c->close_geom.w = th;
	c->close_geom.h = th;

	c->resize_n_geom.x = c->resize_nw_geom.x + c->resize_nw_geom.w;
	c->resize_n_geom.y = 0;
	c->resize_n_geom.w = c->geom.w - (c->close_geom.w * 2);
	c->resize_n_geom.h = bw;

	c->resize_ne_geom.x = c->resize_n_geom.x + c->resize_n_geom.w;
	c->resize_ne_geom.y = 0;
	c->resize_ne_geom.w = bw + th;
	c->resize_ne_geom.h = bw + th;

	c->iconify_geom.x = c->resize_ne_geom.x - th - 1;
	c->iconify_geom.y = bw;
	c->iconify_geom.w = th;
	c->iconify_geom.h = th;

	c->zoom_geom.x = c->resize_ne_geom.x;
	c->zoom_geom.y = bw;
	c->zoom_geom.w = th;
	c->zoom_geom.h = th;

	c->titlebar_geom.x = c->resize_nw_geom.x + c->resize_nw_geom.w;
	c->titlebar_geom.y = c->resize_n_geom.h;
	c->titlebar_geom.w = c->resize_n_geom.w - th - 1;
	c->titlebar_geom.h = th;

	c->resize_e_geom.x = c->zoom_geom.x + c->zoom_geom.w;
	c->resize_e_geom.y = c->zoom_geom.y + c->zoom_geom.h + 1;
	if (c->shaded)
		c->resize_e_geom.y += th;
	c->resize_e_geom.w = bw;
	c->resize_e_geom.h = c->geom.h - c->zoom_geom.h - 1;

	c->resize_se_geom.x = c->zoom_geom.x;
	c->resize_se_geom.y = c->resize_e_geom.y + c->resize_e_geom.h;
	c->resize_se_geom.w = th + bw;
	c->resize_se_geom.h = th + bw;

	if (c->shaded) {
		c->resize_s_geom.x = 0;
		c->resize_s_geom.y = c->resize_nw_geom.h;
		c->resize_s_geom.w = c->resize_ne_geom.x + c->resize_ne_geom.w;
	} else {
		c->resize_s_geom.x = c->resize_n_geom.x;
		c->resize_s_geom.y = c->resize_se_geom.y + 1 + th;
		c->resize_s_geom.w = c->resize_n_geom.w;
	}
	c->resize_s_geom.h = bw;

	c->resize_sw_geom.x = 0;
	c->resize_sw_geom.y = c->resize_se_geom.y;
	c->resize_sw_geom.w = th + bw;
	c->resize_sw_geom.h = th + bw;

	c->resize_w_geom.x = 0;
	c->resize_w_geom.y = c->resize_e_geom.y;
	c->resize_w_geom.w = c->resize_e_geom.w;
	c->resize_w_geom.h = c->resize_e_geom.h;

	c->frame_geom.x = c->geom.x - c->resize_w_geom.w;
	c->frame_geom.y = c->geom.y - 1 - c->resize_nw_geom.h;
	c->frame_geom.w = c->resize_w_geom.w + c->geom.w + c->resize_e_geom.w;
	if (c->shaded)
		c->frame_geom.h = c->resize_nw_geom.h + c->resize_s_geom.h;
	else
		c->frame_geom.h = c->resize_nw_geom.h + 1 + c->geom.h +
		    c->resize_s_geom.h;
}

int
set_wm_state(client_t *c, unsigned long state)
{
	return set_atoms(c->win, wm_state, wm_state, &state, 1);
}

void
check_states(client_t *c)
{
	Atom state;
	unsigned long read, left;
	int i;

	for (i = 0, left = 1; left; i += read) {
		read = get_atoms(c->win, net_wm_state, XA_ATOM, i, &state, 1,
		    &left);
		if (!read)
			break;

		if (state == net_wm_state_shaded)
			shade_client(c);
		else if (state == net_wm_state_mh || state == net_wm_state_mv)
			zoom_client(c);
		else if (state == net_wm_state_fs)
			fullscreen_client(c);
	}
	flush_expose_client(c);
}

/* If we frob the geom for some reason, we need to inform the client. */
void
send_config(client_t *c)
{
	XConfigureEvent ce;

	ce.type = ConfigureNotify;
	ce.event = c->win;
	ce.window = c->win;
	ce.x = c->geom.x;
	ce.y = c->geom.y;
	ce.width = c->geom.w;
	ce.height = c->geom.h;
	ce.border_width = 0;
	ce.above = None;
	ce.override_redirect = 0;

	XSendEvent(dpy, c->win, False, StructureNotifyMask, (XEvent *)&ce);
}

void
redraw_frame(client_t *c)
{
	XftColor *txft;
	XGlyphInfo extents;
	int x, y, tw;

	if (!(c && c->decor && c->frame))
		return;

	recalc_frame(c);

	XMoveResizeWindow(dpy, c->frame,
	    c->frame_geom.x, c->frame_geom.y,
	    c->frame_geom.w, c->frame_geom.h);
	XResizeWindow(dpy, c->win, c->geom.w, c->geom.h);

	/* title bar */
	if (c == focused) {
		txft = &xft_fg;
		XSetWindowBackground(dpy, c->titlebar, bg.pixel);
	} else {
		txft = &xft_fg_unfocused;
		XSetWindowBackground(dpy, c->titlebar, bg_unfocused.pixel);
	}
	XMoveResizeWindow(dpy, c->titlebar,
	    c->titlebar_geom.x, c->titlebar_geom.y,
	    c->titlebar_geom.w, c->titlebar_geom.h);
	XClearWindow(dpy, c->titlebar);

	if (c->name) {
		XftTextExtentsUtf8(dpy, xftfont, (FcChar8 *)c->name,
		    strlen(c->name), &extents);
		tw = extents.xOff;
		x = opt_pad * 2;

		if (tw < (c->titlebar_geom.w - (opt_pad * 2)))
			/* center title */
			x = (c->titlebar_geom.w / 2) - (tw / 2);

		y = opt_pad + xftfont->ascent;

		XftDrawStringUtf8(c->xftdraw, txft, xftfont, x, y,
		    (unsigned char *)c->name, strlen(c->name));
	}

	XSetForeground(dpy, DefaultGC(dpy, screen), BlackPixel(dpy, screen));

	/* close box */
	XSetWindowBackground(dpy, c->close, button_bg.pixel);
	XClearWindow(dpy, c->close);
	XMoveResizeWindow(dpy, c->close,
	    c->close_geom.x, c->close_geom.y, c->close_geom.w, c->close_geom.h);
	x = (c->close_geom.w / 2) - (close_pm_attrs.width / 2);
	y = (c->close_geom.h / 2) - (close_pm_attrs.height / 2);
	XSetClipMask(dpy, pixmap_gc, close_pm_mask);
	XSetClipOrigin(dpy, pixmap_gc, x, y);
	XCopyArea(dpy, close_pm, c->close, pixmap_gc, 0, 0,
	    close_pm_attrs.width, close_pm_attrs.height, x, y);
	if (c->close_pressed)
		XCopyArea(dpy, c->close, c->close, invert_gc, 0, 0,
		    c->close_geom.w, c->close_geom.h, 0, 0);
	else
		XCopyArea(dpy, c->close, c->close, pixmap_gc, 0, 0,
		    c->close_geom.w, c->close_geom.h, 0, 0);

	/* separators between titlebar and boxes */
	XDrawRectangle(dpy, c->titlebar, DefaultGC(dpy, screen),
	    0, -1, c->titlebar_geom.w - 1, c->titlebar_geom.h + 1);
	XDrawRectangle(dpy, c->resize_n, DefaultGC(dpy, screen),
	    0, 0, c->resize_n_geom.w - 1, c->resize_n_geom.h);

	/* iconify box */
	XSetWindowBackground(dpy, c->iconify, button_bg.pixel);
	XClearWindow(dpy, c->iconify);
	XMoveResizeWindow(dpy, c->iconify,
	    c->iconify_geom.x, c->iconify_geom.y, c->iconify_geom.w,
	    c->iconify_geom.h);
	x = (c->iconify_geom.w / 2) - (iconify_pm_attrs.width / 2) -
	    (opt_bevel / 2);
	y = (c->iconify_geom.h / 2) - (iconify_pm_attrs.height / 2) -
	    (opt_bevel / 2);
	if (c->iconify_pressed) {
		x += 2;
		y += 2;
	}
	XSetClipMask(dpy, pixmap_gc, iconify_pm_mask);
	XSetClipOrigin(dpy, pixmap_gc, x, y);
	XCopyArea(dpy, iconify_pm, c->iconify, pixmap_gc, 0, 0,
	    iconify_pm_attrs.width, iconify_pm_attrs.height, x, y);
	bevel(c->iconify, c->iconify_pressed);

	/* zoom box */
	XMoveResizeWindow(dpy, c->zoom,
	    c->zoom_geom.x, c->zoom_geom.y, c->zoom_geom.w, c->zoom_geom.h);
	XSetWindowBackground(dpy, c->zoom, button_bg.pixel);
	XClearWindow(dpy, c->zoom);
	if (c->zoomed) {
		x = (c->zoom_geom.w / 2) - (unzoom_pm_attrs.width / 2) -
		    (opt_bevel / 2);
		y = (c->zoom_geom.h / 2) - (unzoom_pm_attrs.height / 2) -
		    (opt_bevel / 2);
		if (c->zoom_pressed) {
			x += 2;
			y += 2;
		}
		XSetClipMask(dpy, pixmap_gc, unzoom_pm_mask);
		XSetClipOrigin(dpy, pixmap_gc, x, y);
		XCopyArea(dpy, unzoom_pm, c->zoom, pixmap_gc, 0, 0,
		    unzoom_pm_attrs.width, unzoom_pm_attrs.height, x, y);
	} else {
		x = (c->zoom_geom.w / 2) - (zoom_pm_attrs.width / 2) -
		    (opt_bevel / 2);
		y = (c->zoom_geom.h / 2) - (zoom_pm_attrs.height / 2) -
		    (opt_bevel / 2);
		if (c->zoom_pressed) {
			x += 2;
			y += 2;
		}
		XSetClipMask(dpy, pixmap_gc, zoom_pm_mask);
		XSetClipOrigin(dpy, pixmap_gc, x, y);
		XCopyArea(dpy, zoom_pm, c->zoom, pixmap_gc, 0, 0,
		    zoom_pm_attrs.width, zoom_pm_attrs.height, x, y);
	}
	bevel(c->zoom, c->zoom_pressed);

	/* frame outline */
	XSetWindowBackground(dpy, c->resize_nw, button_bg.pixel);
	XClearWindow(dpy, c->resize_nw);
	XMoveResizeWindow(dpy, c->resize_nw,
	    c->resize_nw_geom.x, c->resize_nw_geom.y,
	    c->resize_nw_geom.w, c->resize_nw_geom.h);
	XDrawRectangle(dpy, c->resize_nw, DefaultGC(dpy, screen),
	    0, 0, c->resize_nw_geom.w, c->resize_nw_geom.h);
	XDrawRectangle(dpy, c->resize_nw, DefaultGC(dpy, screen),
	    c->resize_nw_geom.w - c->close_geom.w - 1,
	    c->resize_nw_geom.h - c->close_geom.h - 1,
	    c->resize_nw_geom.w, c->resize_nw_geom.h);

	XSetWindowBackground(dpy, c->resize_w, button_bg.pixel);
	XClearWindow(dpy, c->resize_w);
	XMoveResizeWindow(dpy, c->resize_w,
	    c->resize_w_geom.x, c->resize_w_geom.y,
	    c->resize_w_geom.w, c->resize_w_geom.h);
	XDrawRectangle(dpy, c->resize_w, DefaultGC(dpy, screen),
	    0, -1, c->resize_w_geom.w - 1, c->resize_w_geom.h + 1);

	XSetWindowBackground(dpy, c->resize_sw, button_bg.pixel);
	XClearWindow(dpy, c->resize_sw);
	XMoveResizeWindow(dpy, c->resize_sw,
	    c->resize_sw_geom.x, c->resize_sw_geom.y,
	    c->resize_sw_geom.w, c->resize_sw_geom.h);
	XDrawRectangle(dpy, c->resize_sw, DefaultGC(dpy, screen),
	    0, 0, c->resize_sw_geom.w, c->resize_sw_geom.h);
	XFillRectangle(dpy, c->resize_sw, DefaultGC(dpy, screen),
	    c->resize_w_geom.w - 1, 0,
	    c->resize_sw_geom.w - c->resize_w_geom.w + 1,
	    c->resize_sw_geom.h - c->resize_w_geom.w + 2);

	XSetWindowBackground(dpy, c->resize_s, button_bg.pixel);
	XClearWindow(dpy, c->resize_s);
	XMoveResizeWindow(dpy, c->resize_s,
	    c->resize_s_geom.x, c->resize_s_geom.y,
	    c->resize_s_geom.w, c->resize_s_geom.h);
	XDrawRectangle(dpy, c->resize_s, DefaultGC(dpy, screen),
	    0, 0, c->resize_s_geom.w - 1, c->resize_s_geom.h - 1);

	if (c->shaded) {
		XSetForeground(dpy, DefaultGC(dpy, screen), button_bg.pixel);
		XDrawLine(dpy, c->resize_s, DefaultGC(dpy, screen),
		    1, 0, c->resize_s_geom.h - 1, 0);
		XDrawLine(dpy, c->resize_s, DefaultGC(dpy, screen),
		    c->resize_s_geom.w - c->resize_s_geom.h + 1, 0,
		    c->resize_s_geom.w - 1, 0);

		XSetForeground(dpy, DefaultGC(dpy, screen),
		    BlackPixel(dpy, screen));
		XDrawLine(dpy, c->resize_s, DefaultGC(dpy, screen),
		    c->resize_sw_geom.w, 0,
		    c->resize_sw_geom.w, c->resize_s_geom.h);
		XDrawLine(dpy, c->resize_s, DefaultGC(dpy, screen),
		    c->resize_s_geom.w - c->resize_sw_geom.w - 1, 0,
		    c->resize_s_geom.w - c->resize_sw_geom.w - 1,
		    c->resize_s_geom.h);
	}

	XSetWindowBackground(dpy, c->resize_se, button_bg.pixel);
	XClearWindow(dpy, c->resize_se);
	XMoveResizeWindow(dpy, c->resize_se,
	    c->resize_se_geom.x, c->resize_se_geom.y,
	    c->resize_se_geom.w, c->resize_se_geom.h);
	XDrawRectangle(dpy, c->resize_se, DefaultGC(dpy, screen),
	    -1, 0, c->resize_se_geom.w, c->resize_se_geom.h);
	XFillRectangle(dpy, c->resize_se, DefaultGC(dpy, screen),
	    0, 0,
	    c->resize_se_geom.w - c->resize_e_geom.w + 1,
	    c->resize_se_geom.h - c->resize_e_geom.w + 2);

	XSetWindowBackground(dpy, c->resize_e, button_bg.pixel);
	XClearWindow(dpy, c->resize_e);
	XMoveResizeWindow(dpy, c->resize_e,
	    c->resize_e_geom.x, c->resize_e_geom.y,
	    c->resize_e_geom.w, c->resize_e_geom.h);
	XDrawRectangle(dpy, c->resize_e, DefaultGC(dpy, screen),
	    0, -1, c->resize_e_geom.w - 1, c->resize_e_geom.h + 1);

	XSetWindowBackground(dpy, c->resize_ne, button_bg.pixel);
	XClearWindow(dpy, c->resize_ne);
	XMoveResizeWindow(dpy, c->resize_ne,
	    c->resize_ne_geom.x, c->resize_ne_geom.y,
	    c->resize_ne_geom.w, c->resize_ne_geom.h);
	XDrawRectangle(dpy, c->resize_ne, DefaultGC(dpy, screen),
	    -1, 0, c->resize_ne_geom.w, c->resize_ne_geom.h);
	XDrawRectangle(dpy, c->resize_ne, DefaultGC(dpy, screen),
	    -1, c->resize_ne_geom.w - c->iconify_geom.w - 1,
	    c->iconify_geom.w + 1, c->iconify_geom.h);

	XSetWindowBackground(dpy, c->resize_n, button_bg.pixel);
	XClearWindow(dpy, c->resize_n);
	XMoveResizeWindow(dpy, c->resize_n,
	    c->resize_n_geom.x, c->resize_n_geom.y,
	    c->resize_n_geom.w, c->resize_n_geom.h);
	XDrawRectangle(dpy, c->resize_n, DefaultGC(dpy, screen),
	    0, 0, c->resize_n_geom.w - 1, c->resize_n_geom.h - 1);

	flush_expose_client(c);
}

static void
bevel(Window win, int pressed)
{
	XWindowAttributes attr;
	int x;

	XGetWindowAttributes(dpy, win, &attr);

	/* TODO: store current foreground color */

	XSetForeground(dpy, DefaultGC(dpy, screen), bevel_dark.pixel);

	if (pressed) {
		for (x = 0; x < opt_bevel - 1; x++) {
			XDrawLine(dpy, win, DefaultGC(dpy, screen),
			    x, x, attr.width - x, x);
			XDrawLine(dpy, win, DefaultGC(dpy, screen),
			    x, x, x, attr.height - x);
		}
	} else {
		for (x = 1; x <= opt_bevel; x++) {
			XDrawLine(dpy, win, DefaultGC(dpy, screen),
			    attr.width - x, x - 1,
			    attr.width - x, attr.height - 1);
			XDrawLine(dpy, win, DefaultGC(dpy, screen),
			    x - 1, attr.height - x,
			    attr.width, attr.height - x);
		}

		XSetForeground(dpy, DefaultGC(dpy, screen), bevel_light.pixel);
		for (x = 1; x <= opt_bevel - 1; x++) {
			XDrawLine(dpy, win, DefaultGC(dpy, screen),
			    0, x - 1,
			    attr.width - x, x - 1);
			XDrawLine(dpy, win, DefaultGC(dpy, screen),
			    x - 1, 0,
			    x - 1, attr.height - x);
		}
	}

	XSetForeground(dpy, DefaultGC(dpy, screen), BlackPixel(dpy, screen));
}

void
collect_struts(client_t *c, strut_t *s)
{
	client_t *p;
	XWindowAttributes attr;
	strut_t temp;

	for (p = head; p; p = p->next) {
		if (!IS_ON_CUR_DESK(p) || p == c)
			continue;

		XGetWindowAttributes(dpy, p->win, &attr);
		if (attr.map_state == IsViewable && get_strut(p->win, &temp)) {
			if (temp.left > s->left)
				s->left = temp.left;
			if (temp.right > s->right)
				s->right = temp.right;
			if (temp.top > s->top)
				s->top = temp.top;
			if (temp.bottom > s->bottom)
				s->bottom = temp.bottom;
		}
	}
}

/*
 * Well, the man pages for the shape extension say nothing, but I was able to
 * find a shape.PS.Z on the x.org FTP site. What we want to do here is make the
 * window shape be a boolean OR (or union) of the client's shape and our bar
 * for the name. The bar requires both a bound and a clip because it has a
 * border; the server will paint the border in the region between the two.
 */
void
set_shape(client_t *c)
{
	int n, order;
	XRectangle temp, *rects;

	rects = XShapeGetRectangles(dpy, c->win, ShapeBounding, &n, &order);

	if (n > 1) {
		XShapeCombineShape(dpy, c->frame, ShapeBounding,
		    0, titlebar_height(c), c->win, ShapeBounding, ShapeSet);
		temp.x = -BW(c);
		temp.y = -BW(c);
		temp.width = c->geom.w + 2 * BW(c);
		temp.height = titlebar_height(c) + BW(c);
		XShapeCombineRectangles(dpy, c->frame, ShapeBounding,
		    0, 0, &temp, 1, ShapeUnion, YXBanded);
		temp.x = 0;
		temp.y = 0;
		temp.width = c->geom.w;
		temp.height = titlebar_height(c) - BW(c);
		XShapeCombineRectangles(dpy, c->frame, ShapeClip,
		    0, titlebar_height(c), &temp, 1, ShapeUnion, YXBanded);
		c->shaped = 1;
	} else if (c->shaped) {
		/* I can't find a "remove all shaping" function... */
		temp.x = -BW(c);
		temp.y = -BW(c);
		temp.width = c->geom.w + 2 * BW(c);
		temp.height = c->geom.h + titlebar_height(c) + 2 * BW(c);
		XShapeCombineRectangles(dpy, c->frame, ShapeBounding,
		    0, 0, &temp, 1, ShapeSet, YXBanded);
	}

	XFree(rects);
}

/*
 * I've decided to carefully ignore any errors raised by this function, rather
 * that attempt to determine asychronously if a window is "valid". Xlib calls
 * should only fail here if that a window has removed itself completely before
 * the Unmap and Destroy events get through the queue to us. It's not pretty.
 *
 * The 'withdrawing' argument specifes if the client is actually destroying
 * itself or being destroyed by us, or if we are merely cleaning up its data
 * structures when we exit mid-session.
 */
void
del_client(client_t *c, int mode)
{
	client_t *p;

	XGrabServer(dpy);
	XSetErrorHandler(ignore_xerror);

#ifdef DEBUG
	dump_name(c, "removing", 'r');
	dump_removal(c, mode);
#endif

	if (mode == DEL_WITHDRAW) {
		set_wm_state(c, WithdrawnState);
	} else {	/* mode == DEL_REMAP */
		if (c->zoomed) {
			c->geom.x = c->save.x;
			c->geom.y = c->save.y;
			c->geom.w = c->save.w;
			c->geom.h = c->save.h;
			XResizeWindow(dpy, c->win, c->geom.w, c->geom.h);
		}
		XMapWindow(dpy, c->win);
	}

	remove_atom(root, net_client_list, XA_WINDOW, c->win);
	remove_atom(root, net_client_stack, XA_WINDOW, c->win);

	XSetWindowBorderWidth(dpy, c->win, c->old_bw);
	if (c->xftdraw)
		XftDrawDestroy(c->xftdraw);

	XReparentWindow(dpy, c->win, root, c->geom.x, c->geom.y);
	XRemoveFromSaveSet(dpy, c->win);
	if (c->resize_nw)
		XDestroyWindow(dpy, c->resize_nw);
	if (c->resize_n)
		XDestroyWindow(dpy, c->resize_n);
	if (c->resize_ne)
		XDestroyWindow(dpy, c->resize_ne);
	if (c->resize_e)
		XDestroyWindow(dpy, c->resize_e);
	if (c->resize_se)
		XDestroyWindow(dpy, c->resize_se);
	if (c->resize_s)
		XDestroyWindow(dpy, c->resize_s);
	if (c->resize_sw)
		XDestroyWindow(dpy, c->resize_sw);
	if (c->resize_w)
		XDestroyWindow(dpy, c->resize_w);
	if (c->titlebar)
		XDestroyWindow(dpy, c->titlebar);
	if (c->close)
		XDestroyWindow(dpy, c->close);
	if (c->iconify)
		XDestroyWindow(dpy, c->iconify);
	if (c->zoom)
		XDestroyWindow(dpy, c->zoom);
	XDestroyWindow(dpy, c->frame);

	if (head == c)
		head = c->next;
	else {
		for (p = head; p && p->next; p = p->next)
			if (p->next == c)
				p->next = c->next;
	}

	if (c->name)
		XFree(c->name);
	free(c);

	if (c == focused) {
		focused = NULL;
		focus_client(prev_focused());
	}

	XSync(dpy, False);
	XSetErrorHandler(handle_xerror);
	XUngrabServer(dpy);
}
