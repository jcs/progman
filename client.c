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
#ifdef DEBUG
#include <stdio.h>
#endif
#include <string.h>
#include <err.h>
#include <X11/Xatom.h>
#include <X11/extensions/shape.h>
#ifdef USE_GDK_PIXBUF
#include <gdk-pixbuf-xlib/gdk-pixbuf-xlib.h>
#endif
#include "progman.h"
#include "atom.h"

static void init_geom(client_t *, strut_t *);
static void reparent(client_t *, strut_t *);
static void bevel(Window, geom_t, int);
static void *word_wrap_xft(char *, char, XftFont *, int, int *);

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

	c = malloc(sizeof *c);
	memset(c, 0, sizeof(*c));

	c->name = get_wm_name(w);
	c->icon_name = get_wm_icon_name(w);
	c->win = w;

	update_size_hints(c);
	XGetTransientForHint(dpy, c->win, &c->trans);

	ignore_xerrors++;
	XGetWindowAttributes(dpy, c->win, &attr);
	ignore_xerrors--;
	c->geom.x = attr.x;
	c->geom.y = attr.y;
	c->geom.w = attr.width;
	c->geom.h = attr.height;
	c->cmap = attr.colormap;
	c->old_bw = attr.border_width;

	if (get_atoms(c->win, net_wm_desk, XA_CARDINAL, 0, &c->desk, 1, NULL)) {
		if (c->desk == -1)
			c->desk = DESK_ALL;	/* FIXME */
		if (c->desk >= ndesks && c->desk != DESK_ALL)
			c->desk = cur_desk;
	} else {
		set_atoms(c->win, net_wm_desk, XA_CARDINAL, &cur_desk, 1);
		c->desk = cur_desk;
	}

	/*
	 * We are not actually keeping the stack one in order. However, every
	 * fancy panel uses it and nothing else, no matter what the spec says.
	 * (I'm not sure why, as rearranging the list every time the stacking
	 * changes would be distracting. GNOME's window list applet doesn't.)
	 */
	append_atoms(root, net_client_list, XA_WINDOW, &c->win, 1);
	append_atoms(root, net_client_stack, XA_WINDOW, &c->win, 1);

	/* setup for alt+click dragging */
	XGrabButton(dpy, Button1, Mod1Mask, c->win, True,
	    ButtonPressMask | ButtonReleaseMask, GrabModeAsync, GrabModeAsync,
	    None, move_curs);

	check_states(c);

	if (c->wm_hints)
		XFree(c->wm_hints);
	c->wm_hints = XGetWMHints(dpy, c->win);
	if (c->wm_hints && (c->wm_hints->flags & StateHint) &&
	    c->wm_hints->initial_state == IconicState)
		c->state = STATE_ICONIFIED;

#ifdef DEBUG
	dump_name(c, __func__, "", c->name);
	dump_geom(c, c->geom, "XGetWindowAttributes");
	dump_info(c);
#endif

	return c;
}

client_t *
find_client(Window w, int mode)
{
	client_t *c;

	for (c = focused; c; c = c->next) {
		switch (mode) {
		case MATCH_ANY:
			if (w == c->frame || w == c->win || w == c->resize_nw ||
			    w == c->resize_w || w == c->resize_sw ||
			    w == c->resize_s || w == c->resize_se ||
			    w == c->resize_e || w == c->resize_ne ||
			    w == c->resize_n || w == c->titlebar ||
			    w == c->close || w == c->iconify || w == c->zoom ||
			    w == c->icon || w == c->icon_label)
				return c;
			break;
		case MATCH_FRAME:
			if (w == c->frame || w == c->resize_nw ||
			    w == c->resize_w || w == c->resize_sw ||
			    w == c->resize_s || w == c->resize_se ||
			    w == c->resize_e || w == c->resize_ne ||
			    w == c->resize_n || w == c->titlebar ||
			    w == c->close || w == c->iconify || w == c->zoom ||
			    w == c->icon || w == c->icon_label)
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
find_client_at_coords(Window w, int x, int y)
{
	unsigned int nwins, i;
	Window qroot, qparent, *wins;
	XWindowAttributes attr;
	client_t *c, *foundc = NULL;

	XQueryTree(dpy, root, &qroot, &qparent, &wins, &nwins);
	for (i = nwins - 1; i > 0; i--) {
		ignore_xerrors++;
		XGetWindowAttributes(dpy, wins[i], &attr);
		ignore_xerrors--;
		if (!(c = find_client(wins[i], MATCH_ANY)))
			continue;

		if (c->state & STATE_ICONIFIED) {
			if (x >= c->icon_geom.x &&
			    x <= c->icon_geom.x + c->icon_geom.w &&
			    y >= c->icon_geom.y &&
			    y <= c->icon_geom.y + c->icon_geom.h) {
				foundc = c;
				break;
			}
			if (x >= c->icon_label_geom.x &&
			    x <= c->icon_label_geom.x + c->icon_label_geom.w &&
			    y >= c->icon_label_geom.y &&
			    y <= c->icon_label_geom.y + c->icon_label_geom.h) {
				foundc = c;
				break;
			}
		} else {
			if (x >= c->frame_geom.x &&
			    x <= c->frame_geom.x + c->frame_geom.w &&
			    y >= c->frame_geom.y &&
			    y <= c->frame_geom.y + c->frame_geom.h) {
				foundc = c;
				break;
			}
		}
	}
	XFree(wins);

	return foundc;
}

client_t *
top_client(void)
{
	unsigned int nwins, i;
	Window qroot, qparent, *wins;
	XWindowAttributes attr;
	client_t *c, *foundc = NULL;

	XQueryTree(dpy, root, &qroot, &qparent, &wins, &nwins);
	for (i = nwins - 1; i > 0; i--) {
		ignore_xerrors++;
		XGetWindowAttributes(dpy, wins[i], &attr);
		ignore_xerrors--;
		if ((c = find_client(wins[i], MATCH_FRAME)) &&
		    !(c->state & STATE_ICONIFIED)) {
		    	foundc = c;
			break;
		}
	}
	XFree(wins);

	return foundc;
}

void
map_client(client_t *c)
{
	strut_t s = { 0 };
	int want_raise = 0;

	XGrabServer(dpy);

	collect_struts(c, &s);
	init_geom(c, &s);

	/* this also builds (but does not map) the frame windows */
	reparent(c, &s);

	constrain_frame(c);

	if (shape_support)
		set_shape(c);

	if (c->state & STATE_ICONIFIED) {
		c->ignore_unmap++;
		set_wm_state(c, IconicState);
		XUnmapWindow(dpy, c->win);
		iconify_client(c);
	} else {
		/* we're not allowing WithdrawnState */
		set_wm_state(c, NormalState);
		if (!(c->state & STATE_DOCK))
			want_raise = 1;
	}

	if (c->name)
		XFree(c->name);
	c->name = get_wm_name(c->win);

	if (c->icon_name)
		XFree(c->icon_name);
	c->icon_name = get_wm_icon_name(c->win);

	if (c->state & STATE_ICONIFIED) {
		XResizeWindow(dpy, c->win, c->geom.w, c->geom.h);
		send_config(c);
		adjust_client_order(c, ORDER_ICONIFIED_TOP);
	} else {
		/* we haven't drawn anything yet, setup at the right place */
		recalc_frame(c);
		XMoveResizeWindow(dpy, c->frame,
		    c->frame_geom.x, c->frame_geom.y,
		    c->frame_geom.w, c->frame_geom.h);
		XMoveResizeWindow(dpy, c->win,
		    c->geom.x - c->frame_geom.x, c->geom.y - c->frame_geom.y,
		    c->geom.w, c->geom.h);

		if (want_raise) {
			XMapWindow(dpy, c->frame);
			XMapWindow(dpy, c->win);
			focus_client(c, FOCUS_FORCE);
		} else {
			XMapWindow(dpy, c->frame);
			XMapWindow(dpy, c->win);
			adjust_client_order(c, ORDER_BOTTOM);
			redraw_frame(c, None);
		}

		send_config(c);
		flush_expose_client(c);
	}

	XSync(dpy, False);
	XUngrabServer(dpy);
}

void
update_size_hints(client_t *c)
{
	long supplied;

	XGetWMNormalHints(dpy, c->win, &c->size_hints, &supplied);

	/* Discard bogus hints */
	if ((c->size_hints.flags & PAspect) &&
	    (c->size_hints.min_aspect.x < 1 || c->size_hints.min_aspect.y < 1 ||
	    c->size_hints.max_aspect.x < 1 || c->size_hints.max_aspect.y < 1))
		c->size_hints.flags &= ~PAspect;
	if ((c->size_hints.flags & PMaxSize) && c->size_hints.max_width < 1)
		c->size_hints.flags &= ~PMaxSize;
	if ((c->size_hints.flags & PMinSize) && c->size_hints.min_width < 1)
		c->size_hints.flags &= ~PMinSize;
	if ((c->size_hints.flags & PResizeInc) &&
	    (c->size_hints.width_inc < 1 || c->size_hints.height_inc < 1))
		c->size_hints.flags &= ~PResizeInc;
	if ((c->size_hints.flags & (USSize | PSize)) &&
	    (c->size_hints.width < 1 || c->size_hints.height < 1))
		c->size_hints.flags &= ~(USSize|PSize);
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
#ifdef DEBUG
	geom_t size_flags = { 0 };
#endif
	unsigned long win_type, read, left;
	int screen_x = DisplayWidth(dpy, screen);
	int screen_y = DisplayHeight(dpy, screen);
	int wmax = screen_x - s->left - s->right;
	int hmax = screen_y - s->top - s->bottom;
	int mouse_x, mouse_y;
	int i;

	if (c->state & (STATE_ZOOMED | STATE_FULLSCREEN)) {
		/*
		 * For zoomed windows, we'll adjust later to accommodate the
		 * titlebar.
		 */
		c->geom.x = s->top;
		c->geom.y = s->left;
		c->geom.w = wmax;
		c->geom.h = hmax;
#ifdef DEBUG
		dump_geom(c, c->geom, "init_geom zoom/fs");
#endif
		return;
	}

	/*
	 * If size/position hints are zero but the initial XGetWindowAttributes
	 * reported non-zero, ignore these hint values
	 */
	if (c->size_hints.width == 0 && c->geom.w != 0 &&
	    c->size_hints.height == 0 && c->geom.h != 0)
		c->size_hints.flags &= ~(USSize|PSize);
	if (c->size_hints.x == 0 && c->geom.x != 0 &&
	    c->size_hints.y == 0 && c->geom.y != 0)
		c->size_hints.flags &= ~(USPosition|PPosition);

	/*
	 * Here, we merely set the values; they're in the same place regardless
	 * of whether the user or the program specified them. We'll distinguish
	 * between the two cases later, if we need to.
	 */
	if (c->size_hints.flags & (USSize|PSize)) {
		if (c->size_hints.width >= 0)
			c->geom.w = c->size_hints.width;
		if (c->size_hints.height > 0)
			c->geom.h = c->size_hints.height;

#ifdef DEBUG
		size_flags.w = c->size_hints.width;
		size_flags.h = c->size_hints.height;
		dump_geom(c, c->geom, "init_geom size_hints w/h");
#endif
	}

	if (c->size_hints.flags & (USPosition | PPosition)) {
		if (c->size_hints.x >= 0)
			c->geom.x = c->size_hints.x;
		if (c->size_hints.y >= 0)
			c->geom.y = c->size_hints.y;
#ifdef DEBUG
		size_flags.x = c->size_hints.x;
		size_flags.y = c->size_hints.y;
		dump_geom(c, c->geom, "init_geom size_hints x/y");
#endif
	}

#ifdef DEBUG
	if (c->size_hints.flags & (USSize | PSize | USPosition | PPosition))
		dump_geom(c, size_flags, "init_geom size flags");
#endif

	/*
	 * Several types of windows can put themselves wherever they want, but
	 * we need to read the size hints to get that position before
	 * returning.
	 */
	for (i = 0, left = 1; left; i += read) {
		read = get_atoms(c->win, net_wm_wintype, XA_ATOM, i, &win_type,
		    1, &left);
		if (!read)
			break;
		if (CAN_PLACE_SELF(win_type))
			return;
	}

	if (!c->placed) {
		if (c->geom.x <= 0 && c->geom.y <= 0) {
			/* Place the window near the cursor */
			get_pointer(&mouse_x, &mouse_y);
			recalc_map(c, c->geom, mouse_x, mouse_y, mouse_x,
			    mouse_y, s, NULL);
		} else {
			/*
			 * Place the window's frame where the window requested
			 * to be
			 */
			recalc_frame(c);
			c->geom.x += c->border_width;
			c->geom.y += c->border_width + c->titlebar_geom.h;
		}
	}

	/*
	 * In any case, if we got this far, we need to do a further sanity
	 * check and make sure that the window isn't overlapping any struts --
	 * except for transients, because they might be a panel-type client
	 * popping up a notification window over themselves.
	 */
	if (c->geom.x + c->geom.w > screen_x - s->right)
		c->geom.x = screen_x - s->right - c->geom.w;
	if (c->geom.y + c->geom.h > screen_y - s->bottom)
		c->geom.y = screen_y - s->bottom - c->geom.h;
	if (c->geom.x < s->left || c->geom.w > wmax)
		c->geom.x = s->left;
	if (c->geom.y < s->top || c->geom.h > hmax)
		c->geom.y = s->top;

	recalc_frame(c);

	/* only move already-placed windows if they're off-screen */
	if (c->placed &&
	    (c->frame_geom.x < s->left || c->geom.y <= s->top)) {
		c->geom.x += (c->geom.x - c->frame_geom.x);
		c->geom.y += (c->geom.y - c->frame_geom.y);
		recalc_frame(c);
	}

#ifdef DEBUG
	dump_geom(c, c->geom, __func__);
#endif
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
	pattr.background_pixel = border_bg.pixel;
	pattr.event_mask = SubMask | ButtonPressMask | ButtonReleaseMask |
	    ExposureMask | EnterWindowMask;
	c->frame = XCreateWindow(dpy, root,
	    c->frame_geom.x, c->frame_geom.y,
	    c->frame_geom.w, c->frame_geom.h,
	    0,
	    DefaultDepth(dpy, screen), CopyFromParent,
	    DefaultVisual(dpy, screen),
	    CWOverrideRedirect | CWBackPixel | CWEventMask, &pattr);

	/*
	 * Init all windows to 1x1+1+1 because a width/height of 0 causes a
	 * BadValue error.  redraw_frame moves them to the right size and
	 * position anyway, and some of these never even get mapped/shown.
	 */

#define _(x,y,z) x##y##z
#define CREATE_RESIZE_WIN(DIR) \
	pattr.background_pixel = BlackPixel(dpy, screen); \
	pattr.cursor = _(resize_,DIR,_curs); \
	_(c->resize_,DIR,) = XCreateWindow(dpy, c->frame, 1, 1, 1, 1, \
	    0, CopyFromParent, InputOutput, CopyFromParent, \
	    CWOverrideRedirect | CWBackPixel | CWEventMask | CWCursor, \
	    &pattr); \
	XReparentWindow(dpy, _(c->resize_,DIR,), c->frame, \
	    _(c->resize_,DIR,_geom.x), _(c->resize_,DIR,_geom.y));

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
	c->close = XCreateWindow(dpy, c->frame, 1, 1, 1, 1,
	    0, CopyFromParent, InputOutput, CopyFromParent,
	    CWOverrideRedirect | CWBackPixel | CWEventMask, &pattr);
	XReparentWindow(dpy, c->close, c->frame, c->close_geom.x,
	    c->close_geom.y);

	c->titlebar = XCreateWindow(dpy, c->frame, 1, 1, 1, 1,
	    0, CopyFromParent, InputOutput, CopyFromParent,
	    CWOverrideRedirect | CWBackPixel | CWEventMask, &pattr);
	XReparentWindow(dpy, c->titlebar, c->frame, c->titlebar_geom.x,
	    c->titlebar_geom.y);

	c->iconify = XCreateWindow(dpy, c->frame, 1, 1, 1, 1,
	    0, CopyFromParent, InputOutput, CopyFromParent,
	    CWOverrideRedirect | CWBackPixel | CWEventMask, &pattr);
	XReparentWindow(dpy, c->iconify, c->frame, c->iconify_geom.x,
	    c->iconify_geom.y);

	c->zoom = XCreateWindow(dpy, c->frame, 1, 1, 1, 1,
	    0, CopyFromParent, InputOutput, CopyFromParent,
	    CWOverrideRedirect | CWBackPixel | CWEventMask, &pattr);
	XReparentWindow(dpy, c->zoom, c->frame, c->zoom_geom.x,
	    c->zoom_geom.y);

	c->xftdraw = XftDrawCreate(dpy, (Drawable)c->titlebar,
	    DefaultVisual(dpy, screen), DefaultColormap(dpy, screen));

	if (shape_support)
		XShapeSelectInput(dpy, c->win, ShapeNotifyMask);

	XAddToSaveSet(dpy, c->win);
	XSelectInput(dpy, c->win, ColormapChangeMask | PropertyChangeMask);
	XSetWindowBorderWidth(dpy, c->win, 0);
	XReparentWindow(dpy, c->win, c->frame, c->resize_w_geom.w,
	    c->titlebar_geom.y + c->titlebar_geom.h + 1);
}

int
has_win_type(client_t *c, Atom type)
{
	int i;

	for (i = 0; i < (sizeof(c->win_type) / sizeof(c->win_type[0])); i++) {
		if (c->win_type[i] == type)
			return 1;
	}

	return 0;
}

void
recalc_frame(client_t *c)
{
	int buts = font->ascent + font->descent + (2 * opt_pad) + 2;

	if (buts < close_pm_attrs.width)
	    buts = close_pm_attrs.width;

	if (has_win_type(c, net_wm_type_dock) ||
	    has_win_type(c, net_wm_type_menu) ||
	    has_win_type(c, net_wm_type_splash) ||
	    has_win_type(c, net_wm_type_desk) ||
	    has_win_type(c, kde_net_wm_window_type_override))
		c->frame_style = FRAME_NONE;
	else if (has_win_type(c, net_wm_type_utility))
		c->frame_style = (FRAME_BORDER | FRAME_RESIZABLE |
		    FRAME_CLOSE | FRAME_TITLEBAR);
	else if (c->state & (STATE_DOCK | STATE_FULLSCREEN))
		c->frame_style = FRAME_NONE;
	else if (c->state & STATE_ZOOMED)
		c->frame_style = FRAME_ALL & ~(FRAME_BORDER | FRAME_RESIZABLE);
	else
		c->frame_style = FRAME_ALL;

	if ((c->size_hints.flags & PMinSize) &&
	    (c->size_hints.flags & PMaxSize) &&
	    c->size_hints.min_width == c->size_hints.max_width &&
	    c->size_hints.min_height == c->size_hints.max_height)
		c->frame_style &= ~(FRAME_RESIZABLE | FRAME_ZOOM |
		    FRAME_ICONIFY);

	if (c->frame_style & FRAME_BORDER)
		c->border_width = opt_bw + 2;
	else
		c->border_width = 0;

	if (has_win_type(c, net_wm_type_utility)) {
		/* use tiny titlebar with no window title */
		buts = (2 * opt_pad) + 2;
		if (buts < utility_close_pm_attrs.width)
		    buts = utility_close_pm_attrs.width;
		if (c->frame_style & FRAME_RESIZABLE)
			c->border_width = (opt_bw / 2) + 2;
	}

	if (c->frame_style & FRAME_RESIZABLE) {
		c->resize_nw_geom.x = 0;
		c->resize_nw_geom.y = 0;
		c->resize_nw_geom.w = c->border_width + buts;
		c->resize_nw_geom.h = c->border_width + buts;
	} else
		memset(&c->resize_nw_geom, 0, sizeof(geom_t));

	if (c->frame_style & FRAME_CLOSE) {
		c->close_geom.x = c->border_width - 1;
		c->close_geom.y = c->border_width - 1;
		c->close_geom.w = buts + 1;
		c->close_geom.h = buts + 1;
		if (!(c->frame_style & FRAME_RESIZABLE)) {
			c->close_geom.x++;
			c->close_geom.y++;
			c->close_geom.h--;
			c->close_geom.w--;
		}
	} else
		memset(&c->close_geom, 0, sizeof(geom_t));

	if (c->frame_style & FRAME_RESIZABLE) {
		c->resize_n_geom.x = c->border_width + buts;
		c->resize_n_geom.y = 0;
		c->resize_n_geom.w = c->geom.w - buts - buts;
		c->resize_n_geom.h = c->border_width;
	} else
		memset(&c->resize_n_geom, 0, sizeof(geom_t));

	if (c->frame_style & FRAME_RESIZABLE) {
		c->resize_ne_geom.x = c->border_width + c->geom.w - buts;
		c->resize_ne_geom.y = 0;
		c->resize_ne_geom.w = c->border_width + buts;
		c->resize_ne_geom.h = c->border_width + buts;
	} else
		memset(&c->resize_ne_geom, 0, sizeof(geom_t));

	if (c->frame_style & FRAME_ZOOM) {
		c->zoom_geom.x = c->border_width + c->geom.w - buts;
		c->zoom_geom.y = c->border_width - 1;
		c->zoom_geom.w = buts + 1;
		c->zoom_geom.h = buts + 1;
	} else
		memset(&c->zoom_geom, 0, sizeof(geom_t));

	if (c->frame_style & FRAME_ICONIFY) {
		c->iconify_geom.x = c->border_width + c->geom.w - buts;
		c->iconify_geom.y = c->border_width - 1;
		c->iconify_geom.w = buts + 1;
		c->iconify_geom.h = buts + 1;
		if (c->frame_style & FRAME_ZOOM)
			c->iconify_geom.x -= c->zoom_geom.w - 1;
	} else
		memset(&c->iconify_geom, 0, sizeof(geom_t));

	if (c->frame_style & FRAME_TITLEBAR) {
		c->titlebar_geom.x = c->border_width + c->close_geom.w;
		if (c->frame_style & FRAME_CLOSE)
			c->titlebar_geom.x--;
		c->titlebar_geom.y = c->border_width;
		c->titlebar_geom.w = c->geom.w;
		if (c->frame_style & FRAME_CLOSE)
			c->titlebar_geom.w -= c->close_geom.w - 1;
		if (c->frame_style & FRAME_ICONIFY)
			c->titlebar_geom.w -= c->iconify_geom.w - 2;
		if (c->frame_style & FRAME_ZOOM)
			c->titlebar_geom.w -= c->zoom_geom.w - 2;
		if ((c->frame_style & FRAME_ZOOM) &&
		    (c->frame_style & FRAME_ICONIFY))
			c->titlebar_geom.w++;
		c->titlebar_geom.h = buts;
	} else
		memset(&c->titlebar_geom, 0, sizeof(geom_t));

	if ((c->frame_style & FRAME_RESIZABLE) && !(c->state & STATE_SHADED)) {
		c->resize_e_geom.x = c->border_width + c->geom.w;
		c->resize_e_geom.y = c->border_width + buts;
		c->resize_e_geom.w = c->border_width;
		if (c->frame_style & FRAME_TITLEBAR)
			c->resize_e_geom.h = c->geom.h - buts;
		else
			c->resize_e_geom.h = c->geom.h - buts - buts;
	} else
		memset(&c->resize_e_geom, 0, sizeof(geom_t));

	if ((c->frame_style & FRAME_RESIZABLE) && !(c->state & STATE_SHADED)) {
		c->resize_se_geom.x = c->resize_ne_geom.x;
		c->resize_se_geom.y = c->resize_e_geom.y + c->resize_e_geom.h;
		c->resize_se_geom.w = c->border_width + buts;
		c->resize_se_geom.h = c->border_width + buts;
	} else
		memset(&c->resize_se_geom, 0, sizeof(geom_t));

	if (c->frame_style & FRAME_RESIZABLE) {
		if (c->state & STATE_SHADED) {
			c->resize_s_geom.x = 0;
			c->resize_s_geom.y = c->border_width + buts - 1;
			c->resize_s_geom.w = c->border_width + c->geom.w +
			    c->border_width;
			c->resize_s_geom.h = c->border_width;
		} else {
			c->resize_s_geom.x = c->resize_n_geom.x;
			c->resize_s_geom.y = c->resize_se_geom.y + buts;
			c->resize_s_geom.w = c->resize_n_geom.w;
			c->resize_s_geom.h = c->border_width;
		}
	} else
		memset(&c->resize_s_geom, 0, sizeof(geom_t));

	if ((c->frame_style & FRAME_RESIZABLE) && !(c->state & STATE_SHADED)) {
		c->resize_sw_geom.x = 0;
		c->resize_sw_geom.y = c->resize_se_geom.y;
		c->resize_sw_geom.w = c->resize_se_geom.w;
		c->resize_sw_geom.h = c->resize_se_geom.h;
	} else
		memset(&c->resize_sw_geom, 0, sizeof(geom_t));

	c->resize_w_geom.x = 0;
	c->resize_w_geom.y = c->resize_e_geom.y;
	c->resize_w_geom.w = c->resize_e_geom.w;
	c->resize_w_geom.h = c->resize_e_geom.h;

	c->frame_geom.x = c->geom.x - c->border_width;
	c->frame_geom.y = c->geom.y - c->border_width -
	    ((c->frame_style & FRAME_TITLEBAR) ? buts : 0);
	c->frame_geom.w = c->geom.w + c->border_width + c->border_width;
	if (c->state & STATE_SHADED)
		c->frame_geom.h = c->border_width + buts + c->border_width - 1;
	else
		c->frame_geom.h = c->geom.h + c->border_width +
		    ((c->frame_style & FRAME_TITLEBAR) ? buts : 0) +
		    c->border_width;
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

	/* XXX: c->win is unmapped, we can't talk to it */
	if (c->state & STATE_ICONIFIED)
		return;

	c->state = STATE_NORMAL;
	c->frame_style = FRAME_ALL;

	for (i = 0; i < MAX_WIN_TYPE_ATOMS; i++) {
		if (get_atoms(c->win, net_wm_wintype, XA_ATOM, i,
		    &c->win_type[i], 1, &left)) {
#ifdef DEBUG
			dump_name(c, __func__, "wm_wintype", XGetAtomName(dpy,
			    c->win_type[i]));
#endif
			if (c->win_type[i] == net_wm_type_dock)
				c->state |= STATE_DOCK;
		}

		if (!left)
			break;

		if (left && i == MAX_WIN_TYPE_ATOMS - 1)
			warnx("client has too many _NET_WM_WINDOW_TYPE atoms");
	}

	if (get_wm_state(c->win) == IconicState) {
#ifdef DEBUG
		dump_name(c, __func__, "wm_state", "IconicState");
#endif
		c->state |= STATE_ICONIFIED;
		return;
	}

	for (i = 0, left = 1; left; i += read) {
		read = get_atoms(c->win, net_wm_state, XA_ATOM, i, &state, 1,
		    &left);
		if (!read)
			break;
#ifdef DEBUG
		dump_name(c, __func__, "net_wm_state", XGetAtomName(dpy,
		    state));
#endif
		if (state == net_wm_state_shaded)
			c->state |= STATE_SHADED;
		else if (state == net_wm_state_mh || state == net_wm_state_mv)
			c->state |= STATE_ZOOMED;
		else if (state == net_wm_state_fs)
			c->state |= STATE_FULLSCREEN;
		else if (state == net_wm_state_above)
			c->state |= STATE_ABOVE;
		else if (state == net_wm_state_below)
			c->state |= STATE_BELOW;
	}
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
redraw_frame(client_t *c, Window only)
{
	XftColor *txft;
	XGlyphInfo extents;
	Pixmap *pm, *pm_mask;
	XpmAttributes *pm_attrs;
	int x, y, tw;

	if (!c || (c->frame_style & FRAME_NONE) || !c->frame)
		return;

	if (!IS_ON_CUR_DESK(c))
		return;

	if (c->state & STATE_ICONIFIED) {
		redraw_icon(c, only);
		return;
	}

#ifdef DEBUG
	dump_name(c, __func__, frame_name(c, only), c->name);
#endif

	recalc_frame(c);

	if (only == None) {
		if ((c->frame_style & FRAME_BORDER) &&
		    !(c->frame_style & FRAME_RESIZABLE)) {
			if (c == focused)
				XSetWindowBackground(dpy, c->frame, bg.pixel);
			else
				XSetWindowBackground(dpy, c->frame,
				    unfocused_bg.pixel);
			XClearWindow(dpy, c->frame);

			XSetForeground(dpy, DefaultGC(dpy, screen),
			    WhitePixel(dpy, screen));
			XDrawLine(dpy, c->frame, DefaultGC(dpy, screen),
			    c->border_width, c->border_width,
			    c->frame_geom.w - c->border_width,
			    c->border_width);
			XDrawLine(dpy, c->frame, DefaultGC(dpy, screen),
			    c->frame_geom.w - c->border_width - 1,
			    c->border_width,
			    c->frame_geom.w - c->border_width - 1,
			    c->border_width + c->titlebar_geom.h);
		} else
			XSetWindowBackground(dpy, c->frame,
			    BlackPixel(dpy, screen));

		XMoveResizeWindow(dpy, c->frame,
		    c->frame_geom.x, c->frame_geom.y,
		    c->frame_geom.w, c->frame_geom.h);

		if (c->state & STATE_SHADED)
			/* keep win just below our shaded frame */
			XMoveResizeWindow(dpy, c->win,
			    c->geom.x - c->frame_geom.x,
			    c->geom.y - c->frame_geom.y + c->border_width + 1,
			    c->geom.w, c->geom.h);
		else
			XMoveResizeWindow(dpy, c->win,
			    c->geom.x - c->frame_geom.x,
			    c->geom.y - c->frame_geom.y,
			    c->geom.w, c->geom.h);

		XSetForeground(dpy, DefaultGC(dpy, screen), border_fg.pixel);
		XDrawRectangle(dpy, c->frame, DefaultGC(dpy, screen),
		    0, 0, c->frame_geom.w - 1, c->frame_geom.h - 1);
	}

	if (only == None || only == c->resize_nw) {
		if (c->frame_style & FRAME_RESIZABLE) {
			XSetWindowBackground(dpy, c->resize_nw,
			    border_bg.pixel);
			XClearWindow(dpy, c->resize_nw);
			XMoveResizeWindow(dpy, c->resize_nw,
			    c->resize_nw_geom.x, c->resize_nw_geom.y,
			    c->resize_nw_geom.w, c->resize_nw_geom.h);
			XMapWindow(dpy, c->resize_nw);
			XSetForeground(dpy, DefaultGC(dpy, screen),
			    border_fg.pixel);
			XDrawRectangle(dpy, c->resize_nw,
			    DefaultGC(dpy, screen), 0, 0,
			    c->resize_nw_geom.w - 1, c->resize_nw_geom.h - 1);

			if (!(c->frame_style & FRAME_CLOSE)) {
				XSetForeground(dpy, DefaultGC(dpy, screen),
				    border_fg.pixel);
				XDrawRectangle(dpy, c->resize_nw,
				    DefaultGC(dpy, screen),
				    c->resize_n_geom.h - 1,
				    c->resize_n_geom.h - 1,
				    c->resize_nw_geom.w - 1,
				    c->resize_nw_geom.h - 1);
				XSetForeground(dpy, DefaultGC(dpy, screen),
				    BlackPixel(dpy, screen));
				XFillRectangle(dpy, c->resize_nw,
				    DefaultGC(dpy, screen),
				    c->resize_n_geom.h,
				    c->resize_n_geom.h,
				    c->resize_nw_geom.h - 2,
				    c->resize_nw_geom.h - 2);
			}
		} else
			XUnmapWindow(dpy, c->resize_nw);
	}

	if (only == None || only == c->close) {
		if (c->frame_style & FRAME_CLOSE) {
			XSetWindowBackground(dpy, c->close, button_bg.pixel);
			XClearWindow(dpy, c->close);
			XMoveResizeWindow(dpy, c->close,
			    c->close_geom.x, c->close_geom.y, c->close_geom.w,
			    c->close_geom.h);
			XMapWindow(dpy, c->close);

			if (has_win_type(c, net_wm_type_utility)) {
				pm = &utility_close_pm;
				pm_mask = &utility_close_pm_mask;
				pm_attrs = &utility_close_pm_attrs;
			} else {
				pm = &close_pm;
				pm_mask = &close_pm_mask;
				pm_attrs = &close_pm_attrs;
			}

			x = (c->close_geom.w / 2) - (pm_attrs->width / 2);
			y = (c->close_geom.h / 2) - (pm_attrs->height / 2);
			XSetClipMask(dpy, pixmap_gc, *pm_mask);
			XSetClipOrigin(dpy, pixmap_gc, x, y);
			XCopyArea(dpy, *pm, c->close, pixmap_gc, 0, 0,
			    pm_attrs->width, pm_attrs->height, x, y);
			if (c->close_pressed)
				XCopyArea(dpy, c->close, c->close, invert_gc,
				    0, 0, c->close_geom.w, c->close_geom.h, 0,
				    0);
			else
				XCopyArea(dpy, c->close, c->close, pixmap_gc,
				    0, 0, c->close_geom.w, c->close_geom.h, 0,
				    0);

			XSetForeground(dpy, DefaultGC(dpy, screen),
			    border_fg.pixel);
			XDrawRectangle(dpy, c->close,
			    DefaultGC(dpy, screen),
			    0, 0, c->close_geom.w - 1,
			    c->close_geom.h - 1);

			if ((c->frame_style & FRAME_BORDER) &&
			    !(c->frame_style & FRAME_RESIZABLE)) {
				XSetForeground(dpy, DefaultGC(dpy, screen),
				    WhitePixel(dpy, screen));
				XDrawLine(dpy, c->close,
				    DefaultGC(dpy, screen),
				    0, 0, c->close_geom.w, 0);
				XDrawLine(dpy, c->close,
				    DefaultGC(dpy, screen),
				    0, 0, 0, c->close_geom.h - 1);
			}
		} else
			XUnmapWindow(dpy, c->close);
	}

	if (only == None || only == c->titlebar) {
		if (c->frame_style & FRAME_TITLEBAR) {
			if (c == focused) {
				txft = &xft_fg;
				XSetWindowBackground(dpy, c->titlebar,
				    bg.pixel);
			} else {
				txft = &xft_fg_unfocused;
				XSetWindowBackground(dpy, c->titlebar,
				    unfocused_bg.pixel);
			}
			XMoveResizeWindow(dpy, c->titlebar,
			    c->titlebar_geom.x, c->titlebar_geom.y,
			    c->titlebar_geom.w, c->titlebar_geom.h);
			XMapWindow(dpy, c->titlebar);
			XClearWindow(dpy, c->titlebar);

			if (c->name && !has_win_type(c, net_wm_type_utility)) {
				XftTextExtentsUtf8(dpy, font,
				    (FcChar8 *)c->name, strlen(c->name),
				    &extents);
				tw = extents.xOff;
				x = opt_pad * 2;

				if (tw < (c->titlebar_geom.w - (opt_pad * 2)))
					/* center title */
					x = (c->titlebar_geom.w / 2) - (tw / 2);

				y = opt_pad + font->ascent;

				XftDrawStringUtf8(c->xftdraw, txft, font, x,
				    y, (unsigned char *)c->name,
				    strlen(c->name));
			}
			if (!(c->frame_style & FRAME_RESIZABLE) &&
			    (c->state & STATE_SHADED))
				XSetForeground(dpy, DefaultGC(dpy, screen),
				    WhitePixel(dpy, screen));
			else
				XSetForeground(dpy, DefaultGC(dpy, screen),
				    border_fg.pixel);
			XDrawLine(dpy, c->titlebar, DefaultGC(dpy, screen),
			    0, c->titlebar_geom.h - 1, c->titlebar_geom.w + 1,
			    c->titlebar_geom.h - 1);

			if ((c->frame_style & FRAME_BORDER) &&
			    !(c->frame_style & FRAME_RESIZABLE)) {
				XSetForeground(dpy, DefaultGC(dpy, screen),
				    WhitePixel(dpy, screen));
				XDrawLine(dpy, c->titlebar,
				    DefaultGC(dpy, screen),
				    0, 0, c->titlebar_geom.w, 0);
				XDrawLine(dpy, c->titlebar,
				    DefaultGC(dpy, screen),
				    c->titlebar_geom.w - 1, 0,
				    c->titlebar_geom.w - 1,
				    c->titlebar_geom.h - 1);
			}
		} else
			XUnmapWindow(dpy, c->titlebar);
	}

	if (only == None || only == c->resize_ne) {
		if (c->frame_style & FRAME_RESIZABLE) {
			XSetWindowBackground(dpy, c->resize_ne,
			    border_bg.pixel);
			XMapWindow(dpy, c->resize_ne);
			XClearWindow(dpy, c->resize_ne);
			XMoveResizeWindow(dpy, c->resize_ne,
			    c->resize_ne_geom.x, c->resize_ne_geom.y,
			    c->resize_ne_geom.w, c->resize_ne_geom.h);
			XSetForeground(dpy, DefaultGC(dpy, screen),
			    border_fg.pixel);
			XDrawRectangle(dpy, c->resize_ne,
			    DefaultGC(dpy, screen), 0, 0,
			    c->resize_ne_geom.w - 1, c->resize_ne_geom.h - 1);
			if (!(c->frame_style & (FRAME_ICONIFY | FRAME_ZOOM))) {
				XSetForeground(dpy, DefaultGC(dpy, screen),
				    border_fg.pixel);
				XDrawRectangle(dpy, c->resize_ne,
				    DefaultGC(dpy, screen),
				    0, c->resize_n_geom.h - 1,
				    c->resize_ne_geom.w - c->resize_n_geom.h,
				    c->resize_ne_geom.h - 1);
				XSetForeground(dpy, DefaultGC(dpy, screen),
				    BlackPixel(dpy, screen));
				XFillRectangle(dpy, c->resize_ne,
				    DefaultGC(dpy, screen),
				    0, c->resize_n_geom.h,
				    c->resize_ne_geom.w - c->resize_n_geom.h,
				    c->resize_ne_geom.h - 2);
			}
		} else
			XUnmapWindow(dpy, c->resize_ne);
	}

	if (only == None || only == c->zoom) {
		if (c->frame_style & FRAME_ZOOM) {
			XMoveResizeWindow(dpy, c->zoom,
			    c->zoom_geom.x, c->zoom_geom.y, c->zoom_geom.w,
			    c->zoom_geom.h);
			XSetWindowBackground(dpy, c->zoom, button_bg.pixel);
			XClearWindow(dpy, c->zoom);
			XMapWindow(dpy, c->zoom);

			if (c->state & STATE_ZOOMED) {
				pm = &unzoom_pm;
				pm_mask = &unzoom_pm_mask;
				pm_attrs = &unzoom_pm_attrs;
			} else {
				pm = &zoom_pm;
				pm_mask = &zoom_pm_mask;
				pm_attrs = &zoom_pm_attrs;
			}

			x = (c->zoom_geom.w / 2) - (pm_attrs->width / 2) -
			    (opt_bevel / 2);
			y = (c->zoom_geom.h / 2) - (pm_attrs->height / 2) -
			    (opt_bevel / 2);
			if (c->zoom_pressed) {
				x += 2;
				y += 2;
			}
			XSetClipMask(dpy, pixmap_gc, *pm_mask);
			XSetClipOrigin(dpy, pixmap_gc, x, y);
			XCopyArea(dpy, *pm, c->zoom, pixmap_gc, 0, 0,
			    pm_attrs->width, pm_attrs->height, x, y);
			bevel(c->zoom, c->zoom_geom, c->zoom_pressed);
			XSetForeground(dpy, DefaultGC(dpy, screen),
			    border_fg.pixel);
			XDrawRectangle(dpy, c->zoom, DefaultGC(dpy, screen),
			    0, 0, c->zoom_geom.w - 1, c->zoom_geom.h - 1);
		} else
			XUnmapWindow(dpy, c->zoom);
	}

	if (only == None || only == c->iconify) {
		if (c->frame_style & FRAME_ICONIFY) {
			XSetWindowBackground(dpy, c->iconify, button_bg.pixel);
			XClearWindow(dpy, c->iconify);
			XMoveResizeWindow(dpy, c->iconify,
			    c->iconify_geom.x, c->iconify_geom.y,
			    c->iconify_geom.w,
			    c->iconify_geom.h);
			XMapWindow(dpy, c->iconify);
			x = (c->iconify_geom.w / 2) -
			    (iconify_pm_attrs.width / 2) - (opt_bevel / 2);
			y = (c->iconify_geom.h / 2) -
			    (iconify_pm_attrs.height / 2) - (opt_bevel / 2);
			if (c->iconify_pressed) {
				x += 2;
				y += 2;
			}
			XSetClipMask(dpy, pixmap_gc, iconify_pm_mask);
			XSetClipOrigin(dpy, pixmap_gc, x, y);
			XCopyArea(dpy, iconify_pm, c->iconify, pixmap_gc, 0, 0,
			    iconify_pm_attrs.width, iconify_pm_attrs.height, x,
			    y);
			bevel(c->iconify, c->iconify_geom, c->iconify_pressed);
			XSetForeground(dpy, DefaultGC(dpy, screen),
			    border_fg.pixel);
			XDrawRectangle(dpy, c->iconify, DefaultGC(dpy, screen),
			    0, 0, c->iconify_geom.w - 1, c->iconify_geom.h - 1);
		} else
			XUnmapWindow(dpy, c->iconify);
	}

	if (only == None || only == c->resize_n) {
		if (c->frame_style & FRAME_RESIZABLE) {
			XSetWindowBackground(dpy, c->resize_n, border_bg.pixel);
			XClearWindow(dpy, c->resize_n);
			XMoveResizeWindow(dpy, c->resize_n,
			    c->resize_n_geom.x, c->resize_n_geom.y,
			    c->resize_n_geom.w, c->resize_n_geom.h);
			XMapWindow(dpy, c->resize_n);
			XSetForeground(dpy, DefaultGC(dpy, screen),
			    border_fg.pixel);
			XDrawRectangle(dpy, c->resize_n, DefaultGC(dpy, screen),
			    -1, 0, c->resize_n_geom.w + 1,
			    c->resize_n_geom.h - 1);
		} else
			XUnmapWindow(dpy, c->resize_n);
	}

	if (only == None || only == c->resize_e) {
		if ((c->frame_style & FRAME_RESIZABLE) &&
		    !(c->state & STATE_SHADED)) {
			XSetWindowBackground(dpy, c->resize_e, border_bg.pixel);
			XMapWindow(dpy, c->resize_e);
			XClearWindow(dpy, c->resize_e);
			XMoveResizeWindow(dpy, c->resize_e,
			    c->resize_e_geom.x, c->resize_e_geom.y,
			    c->resize_e_geom.w, c->resize_e_geom.h);
			XSetForeground(dpy, DefaultGC(dpy, screen),
			    border_fg.pixel);
			XDrawRectangle(dpy, c->resize_e, DefaultGC(dpy, screen),
			    0, -1, c->resize_e_geom.w - 1,
			    c->resize_e_geom.h + 1);
		} else
			XUnmapWindow(dpy, c->resize_e);
	}

	if (only == None || only == c->resize_se) {
		if ((c->frame_style & FRAME_RESIZABLE) &&
		    !(c->state & STATE_SHADED)) {
			XSetWindowBackground(dpy, c->resize_se,
			    border_bg.pixel);
			XClearWindow(dpy, c->resize_se);
			XMoveResizeWindow(dpy, c->resize_se,
			    c->resize_se_geom.x, c->resize_se_geom.y,
			    c->resize_se_geom.w, c->resize_se_geom.h);
			XMapWindow(dpy, c->resize_se);
			XSetForeground(dpy, DefaultGC(dpy, screen),
			    border_fg.pixel);
			XDrawRectangle(dpy, c->resize_se,
			    DefaultGC(dpy, screen), 0, 0,
			    c->resize_se_geom.w - 1,
			    c->resize_se_geom.h - 1);
			XDrawRectangle(dpy, c->resize_se,
			    DefaultGC(dpy, screen), 0, 0,
			    c->resize_se_geom.w - c->resize_s_geom.h,
			    c->resize_se_geom.h - c->resize_s_geom.h);
			XSetForeground(dpy, DefaultGC(dpy, screen),
			    BlackPixel(dpy, screen));
			XFillRectangle(dpy, c->resize_se,
			    DefaultGC(dpy, screen), 0, 0,
			    c->resize_se_geom.w - c->resize_s_geom.h,
			    c->resize_se_geom.h - c->resize_s_geom.h);
		} else
			XUnmapWindow(dpy, c->resize_se);
	}

	if (only == None || only == c->resize_s) {
		if (c->frame_style & FRAME_RESIZABLE) {
			XSetWindowBackground(dpy, c->resize_s, border_bg.pixel);
			XClearWindow(dpy, c->resize_s);
			XMoveResizeWindow(dpy, c->resize_s,
			    c->resize_s_geom.x, c->resize_s_geom.y,
			    c->resize_s_geom.w, c->resize_s_geom.h);
			XMapWindow(dpy, c->resize_s);
			XSetForeground(dpy, DefaultGC(dpy, screen),
			    border_fg.pixel);
			XDrawRectangle(dpy, c->resize_s, DefaultGC(dpy, screen),
			    0, 0, c->resize_s_geom.w,
			    c->resize_s_geom.h - 1);

			if (c->state & STATE_SHADED) {
				XSetForeground(dpy, DefaultGC(dpy, screen),
				    border_bg.pixel);
				XDrawLine(dpy, c->resize_s,
				    DefaultGC(dpy, screen), 1, 0,
				    c->resize_s_geom.h - 1, 0);
				XDrawLine(dpy, c->resize_s,
				    DefaultGC(dpy, screen),
				    c->resize_s_geom.w - c->resize_s_geom.h + 1,
				    0, c->resize_s_geom.w - 1, 0);

				XSetForeground(dpy, DefaultGC(dpy, screen),
				    border_fg.pixel);
				XDrawLine(dpy, c->resize_s,
				    DefaultGC(dpy, screen),
				    c->resize_sw_geom.w, 0,
				    c->resize_sw_geom.w, c->resize_s_geom.h);
				XDrawLine(dpy, c->resize_s,
				    DefaultGC(dpy, screen),
				    c->resize_s_geom.w - c->resize_sw_geom.w -
				    1, 0,
				    c->resize_s_geom.w - c->resize_sw_geom.w -
				    1, c->resize_s_geom.h);
			}
		} else
			XUnmapWindow(dpy, c->resize_s);
	}

	if (only == None || only == c->resize_sw) {
		if ((c->frame_style & FRAME_RESIZABLE) &&
		    !(c->state & STATE_SHADED)) {
			XSetWindowBackground(dpy, c->resize_sw,
			    border_bg.pixel);
			XClearWindow(dpy, c->resize_sw);
			XMoveResizeWindow(dpy, c->resize_sw,
			    c->resize_sw_geom.x, c->resize_sw_geom.y,
			    c->resize_sw_geom.w, c->resize_sw_geom.h);
			XMapWindow(dpy, c->resize_sw);
			XSetForeground(dpy, DefaultGC(dpy, screen),
			    border_fg.pixel);
			XDrawRectangle(dpy, c->resize_sw,
			    DefaultGC(dpy, screen), 0, 0, c->resize_sw_geom.w,
			    c->resize_sw_geom.h - 1);
			XDrawRectangle(dpy, c->resize_sw,
			    DefaultGC(dpy, screen),
			    c->resize_w_geom.w - 1, 0,
			    c->resize_sw_geom.w,
			    c->resize_sw_geom.h - c->resize_s_geom.h);
			XSetForeground(dpy, DefaultGC(dpy, screen),
			    BlackPixel(dpy, screen));
			XFillRectangle(dpy, c->resize_sw,
			    DefaultGC(dpy, screen), c->resize_w_geom.w, 0,
			    c->resize_sw_geom.w,
			    c->resize_sw_geom.h - c->resize_s_geom.h);
		} else
			XUnmapWindow(dpy, c->resize_sw);
	}

	if (only == None || only == c->resize_w) {
		if ((c->frame_style & FRAME_RESIZABLE) &&
		    !(c->state & STATE_SHADED)) {
			XSetWindowBackground(dpy, c->resize_w, border_bg.pixel);
			XClearWindow(dpy, c->resize_w);
			XMoveResizeWindow(dpy, c->resize_w,
			    c->resize_w_geom.x, c->resize_w_geom.y,
			    c->resize_w_geom.w, c->resize_w_geom.h);
			XMapWindow(dpy, c->resize_w);
			XSetForeground(dpy, DefaultGC(dpy, screen),
			    border_fg.pixel);
			XDrawRectangle(dpy, c->resize_w, DefaultGC(dpy, screen),
			    0, -1, c->resize_w_geom.w - 1,
			    c->resize_w_geom.h + 1);
		} else
			XUnmapWindow(dpy, c->resize_w);
	}
}

static void
bevel(Window win, geom_t geom, int pressed)
{
	int x;

	XSetForeground(dpy, DefaultGC(dpy, screen), bevel_dark.pixel);

	if (pressed) {
		for (x = 0; x < opt_bevel - 1; x++) {
			XDrawLine(dpy, win, DefaultGC(dpy, screen),
			    x, x, geom.w - x, x);
			XDrawLine(dpy, win, DefaultGC(dpy, screen),
			    x, x, x, geom.h - x);
		}
	} else {
		for (x = 1; x <= opt_bevel; x++) {
			XDrawLine(dpy, win, DefaultGC(dpy, screen),
			    geom.w - 1 - x, x,
			    geom.w - 1 - x, geom.h - 1);
			XDrawLine(dpy, win, DefaultGC(dpy, screen),
			    x, geom.h - 1 - x,
			    geom.w - 1, geom.h - x - 1);
		}

		XSetForeground(dpy, DefaultGC(dpy, screen), bevel_light.pixel);
		for (x = 1; x <= opt_bevel - 1; x++) {
			XDrawLine(dpy, win, DefaultGC(dpy, screen),
			    1, x,
			    geom.w - 1 - x, x);
			XDrawLine(dpy, win, DefaultGC(dpy, screen),
			    x, 1,
			    x, geom.h - 1 - x);
		}
	}
}

void
get_client_icon(client_t *c)
{
	Window junkw;
	unsigned long w, h;
	unsigned int depth;
	int junki;

	/* try through atom */
	if (get_atoms(c->win, net_wm_icon, XA_CARDINAL, 0, &w, 1, NULL) &&
	    get_atoms(c->win, net_wm_icon, XA_CARDINAL, 1, &h, 1, NULL)) {
	    	/* TODO */
	}

	if (c->icon_managed) {
		if (c->icon_pixmap)
			XFreePixmap(dpy, c->icon_pixmap);
		if (c->icon_mask)
			XFreePixmap(dpy, c->icon_mask);
		c->icon_managed = 0;
	}

	/* fallback to WMHints */
	if (c->wm_hints)
		XFree(c->wm_hints);
	c->wm_hints = XGetWMHints(dpy, c->win);
	if (!c->wm_hints || !(c->wm_hints->flags & IconPixmapHint)) {
		c->icon_pixmap = default_icon_pm;
		c->icon_depth = DefaultDepth(dpy, screen);
		c->icon_mask = default_icon_pm_mask;
		return;
	}

	XGetGeometry(dpy, c->wm_hints->icon_pixmap, &junkw, &junki, &junki,
	    (unsigned int *)&c->icon_geom.w, (unsigned int *)&c->icon_geom.h,
	    (unsigned int *)&junki, &depth);
	c->icon_pixmap = c->wm_hints->icon_pixmap;
	c->icon_depth = depth;

	if (c->wm_hints->flags & IconMaskHint)
		c->icon_mask = c->wm_hints->icon_mask;
	else
		c->icon_mask = None;

#ifdef USE_GDK_PIXBUF
	if (c->icon_geom.w > icon_size || c->icon_geom.h > icon_size) {
		GdkPixbuf *gp, *mask, *scaled;
		int sh, sw;

		if (c->icon_geom.w > c->icon_geom.h) {
			sw = icon_size;
			sh = (icon_size / (double)c->icon_geom.w) *
			    c->icon_geom.h;
		} else {
			sh = icon_size;
			sw = (icon_size / (double)c->icon_geom.h) *
			    c->icon_geom.w;
		}

		gp = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8,
		    c->icon_geom.w, c->icon_geom.h);

		if (!gdk_pixbuf_xlib_get_from_drawable(gp, c->icon_pixmap,
		    c->cmap, DefaultVisual(dpy, screen), 0, 0, 0, 0,
		    c->icon_geom.w, c->icon_geom.h)) {
			warnx("failed to load pixmap with gdk pixbuf");
			g_object_unref(gp);
			return;
		}

		/* manually mask image, ugh */
		if (c->icon_mask != None) {
			guchar *px, *pxm;
			int rs, rsm, dm, ch, x, y;

			mask = gdk_pixbuf_xlib_get_from_drawable(NULL,
			    c->icon_mask, c->cmap, DefaultVisual(dpy, screen),
			    0, 0, 0, 0, c->icon_geom.w, c->icon_geom.h);
			if (!mask) {
				warnx("failed to load mask with gdk pixbuf");
				g_object_unref(gp);
				return;
			}

			px = gdk_pixbuf_get_pixels(gp);
			pxm = gdk_pixbuf_get_pixels(mask);
			rs = gdk_pixbuf_get_rowstride(gp);
			rsm = gdk_pixbuf_get_rowstride(mask);
			dm = gdk_pixbuf_get_bits_per_sample(mask);
			ch = gdk_pixbuf_get_n_channels(mask);

			for (y = 0; y < c->icon_geom.h; y++) {
				guchar *tr = px + (y * rs);
				guchar *trm = pxm + (y * rsm);
				for (x = 0; x < c->icon_geom.w; x++) {
					guchar al = 0xff;
					switch (dm) {
					case 1:
						al = trm[x * ch / 8];
						al >>= (x % 8);
						al = al ? 0xff : 0;
						break;
					case 8:
						al = (trm[(x * ch) + 2]) ? 0xff
						    : 0;
						break;
					}

					tr[(x * 4) + 3] = al;
				}
			}

			g_object_unref(mask);
		}

		scaled = gdk_pixbuf_scale_simple(gp, sw, sh,
		    GDK_INTERP_BILINEAR);
		if (!scaled) {
			warnx("failed to scale icon with gdk pixbuf");
			g_object_unref(gp);
			return;
		}

		if (c->icon_managed) {
			if (c->icon_pixmap)
				XFreePixmap(dpy, c->icon_pixmap);
			if (c->icon_mask)
				XFreePixmap(dpy, c->icon_mask);
		}

		gdk_pixbuf_xlib_render_pixmap_and_mask(scaled,
		    &c->icon_pixmap, &c->icon_mask, 1);
		c->icon_geom.w = sw;
		c->icon_geom.h = sh;
		c->icon_managed = 1;

		g_object_unref(scaled);
		g_object_unref(gp);
	}
#endif
}

void
redraw_icon(client_t *c, Window only)
{
	XftColor *txft;
	void *xft_lines;
	int label_pad = 2 * opt_scale;
	int nlines, x;

#ifdef DEBUG
	dump_name(c, __func__, frame_name(c, only), c->name);
#endif

	if (only == None || only == c->icon) {
		XClearWindow(dpy, c->icon);
		XSetWindowBackground(dpy, c->icon, WhitePixel(dpy, screen));
		XMoveResizeWindow(dpy, c->icon,
		    c->icon_geom.x + ((icon_size - c->icon_geom.w) / 2),
		    c->icon_geom.y + ((icon_size - c->icon_geom.h) / 2),
		    c->icon_geom.w, c->icon_geom.h);
		if (c->icon_mask) {
			XShapeCombineMask(dpy, c->icon, ShapeBounding, 0, 0,
			    c->icon_mask, ShapeSet);
			XSetClipMask(dpy, c->icon_gc, c->icon_mask);
			XSetClipOrigin(dpy, c->icon_gc, 0, 0);
		}
		if (c->icon_depth == DefaultDepth(dpy, screen))
			XCopyArea(dpy, c->icon_pixmap, c->icon, c->icon_gc,
			    0, 0, c->icon_geom.w, c->icon_geom.h, 0, 0);
		else {
			XSetBackground(dpy, c->icon_gc,
			    BlackPixel(dpy, screen));
			XSetForeground(dpy, c->icon_gc,
			    WhitePixel(dpy, screen));
			XCopyPlane(dpy, c->icon_pixmap, c->icon, c->icon_gc,
			    0, 0, c->icon_geom.w, c->icon_geom.h, 0, 0, 1);
		}
	}

	if (only != None && only != c->icon_label)
		return;

	if (c == focused) {
		txft = &xft_fg;
		XSetWindowBackground(dpy, c->icon_label, bg.pixel);
		XSetForeground(dpy, DefaultGC(dpy, screen), fg.pixel);
	} else {
		txft = &xft_fg_unfocused;
		XSetWindowBackground(dpy, c->icon_label, unfocused_bg.pixel);
		XSetForeground(dpy, DefaultGC(dpy, screen),
		    unfocused_fg.pixel);
	}

	XClearWindow(dpy, c->icon_label);

	if (!c->icon_name)
		c->icon_name = strdup("(Unknown)");

	xft_lines = word_wrap_xft(c->icon_name, ' ', iconfont,
	    (icon_size * 2) - (label_pad * 2), &nlines);

	c->icon_label_geom.y = c->icon_geom.y + icon_size + 10;
	c->icon_label_geom.h = label_pad;
	c->icon_label_geom.w = label_pad;

	for (x = 0; x < nlines; x++) {
		struct xft_line_t *line = xft_lines +
		    (sizeof(struct xft_line_t) * x);
		int w = label_pad + line->xft_width + label_pad;
		if (w > c->icon_label_geom.w)
			c->icon_label_geom.w = w;
		c->icon_label_geom.h += iconfont->ascent + iconfont->descent;
	}

	c->icon_label_geom.h += label_pad;
	c->icon_label_geom.x = c->icon_geom.x -
	    ((c->icon_label_geom.w - icon_size) / 2);

	XMoveResizeWindow(dpy, c->icon_label,
	    c->icon_label_geom.x, c->icon_label_geom.y,
	    c->icon_label_geom.w, c->icon_label_geom.h);

	int ly = label_pad;
	for (x = 0; x < nlines; x++) {
		struct xft_line_t *line = xft_lines +
		    (sizeof(struct xft_line_t) * x);
		int lx = ((c->icon_label_geom.w - line->xft_width) / 2);

		ly += iconfont->ascent;
		XftDrawStringUtf8(c->icon_xftdraw, txft, iconfont, lx, ly,
		    (FcChar8 *)line->str, line->len);
		ly += iconfont->descent;
	}

	free(xft_lines);
}

void
collect_struts(client_t *c, strut_t *s)
{
	client_t *p;
	XWindowAttributes attr;
	strut_t temp;

	for (p = focused; p; p = p->next) {
		if (!IS_ON_CUR_DESK(p) || p == c)
			continue;

		ignore_xerrors++;
		XGetWindowAttributes(dpy, p->win, &attr);
		ignore_xerrors--;
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
		/* window contents */
		XShapeCombineShape(dpy, c->frame, ShapeBounding,
		    c->resize_w_geom.w,
		    c->resize_n_geom.h + c->titlebar_geom.h,
		    c->win, ShapeBounding, ShapeSet);

		/* titlebar */
		temp.x = 0;
		temp.y = 0;
		temp.width = c->frame_geom.w;
		temp.height = c->resize_n_geom.h + c->titlebar_geom.h;
		XShapeCombineRectangles(dpy, c->frame, ShapeBounding,
		    0, 0, &temp, 1, ShapeUnion, YXBanded);

		/* bottom border */
		temp.height = c->resize_s_geom.h;
		XShapeCombineRectangles(dpy, c->frame, ShapeBounding,
		    0, c->frame_geom.h - c->resize_s_geom.h, &temp, 1,
		    ShapeUnion, YXBanded);

		/* left border */
		temp.width = c->resize_w_geom.w;
		temp.height = c->frame_geom.h;
		XShapeCombineRectangles(dpy, c->frame, ShapeBounding,
		    0, 0, &temp, 1, ShapeUnion, YXBanded);
		/* right border */
		XShapeCombineRectangles(dpy, c->frame, ShapeBounding,
		    c->frame_geom.w - c->resize_e_geom.w, 0, &temp, 1,
		    ShapeUnion, YXBanded);

		c->shaped = 1;
	} else
		c->shaped = 0;

	XFree(rects);
}

/*
 * I've decided to carefully ignore any errors raised by this function, rather
 * that attempt to determine asychronously if a window is "valid". Xlib calls
 * should only fail here if that a window has removed itself completely before
 * the Unmap and Destroy events get through the queue to us. It's not pretty.
 *
 * The mode argument specifes if the client is actually destroying itself or
 * being destroyed by us, or if we are merely cleaning up its data structures
 * when we exit mid-session.
 */
void
del_client(client_t *c, int mode)
{
	client_t *next;

	XSync(dpy, False);
	XGrabServer(dpy);
	ignore_xerrors++;

#ifdef DEBUG
	dump_name(c, __func__, mode == DEL_WITHDRAW ? "withdraw" : "", c->name);
	dump_removal(c, mode);
#endif

	if (mode == DEL_WITHDRAW) {
		set_wm_state(c, WithdrawnState);
		XUnmapWindow(dpy, c->frame);
	} else {
		if (c->state & STATE_ZOOMED) {
			c->geom.x = c->save.x;
			c->geom.y = c->save.y;
			c->geom.w = c->save.w;
			c->geom.h = c->save.h;
			XResizeWindow(dpy, c->win, c->geom.w, c->geom.h);
		}
		XMapWindow(dpy, c->win);
		XSetWindowBorderWidth(dpy, c->win, c->old_bw);
	}

	remove_atom(root, net_client_list, XA_WINDOW, c->win);
	remove_atom(root, net_client_stack, XA_WINDOW, c->win);

	if (c->xftdraw)
		XftDrawDestroy(c->xftdraw);

	XReparentWindow(dpy, c->win, root, c->geom.x, c->geom.y);
	XRemoveFromSaveSet(dpy, c->win);
	XDestroyWindow(dpy, c->frame);

	if (c->icon_xftdraw)
		XftDrawDestroy(c->icon_xftdraw);
	if (c->icon) {
		XDestroyWindow(dpy, c->icon);
		if (c->icon_label)
			XDestroyWindow(dpy, c->icon_label);
	}
	if (c->icon_gc)
		XFreeGC(dpy, c->icon_gc);
	if (c->icon_managed) {
		if (c->icon_pixmap)
			XFreePixmap(dpy, c->icon_pixmap);
		if (c->icon_mask)
			XFreePixmap(dpy, c->icon_mask);
		c->icon_managed = 0;
	}

	if (c->name)
		XFree(c->name);
	if (c->icon_name)
		XFree(c->icon_name);

	if (focused == c) {
		next = next_client_for_focus(focused);
		if (!next)
			next = focused->next;
		adjust_client_order(c, ORDER_OUT);
		if (next)
			focus_client(next, FOCUS_FORCE);
		else
			focused = NULL;
	} else
		adjust_client_order(c, ORDER_OUT);

	free(c);

	XSync(dpy, False);
	ignore_xerrors--;
	XUngrabServer(dpy);
}

void *
word_wrap_xft(char *str, char delim, XftFont *font, int width, int *nlines)
{
	XGlyphInfo extents;
	struct xft_line_t *lines = NULL;
	char *curstr;
	int x, lastdelim;
	int alloced = 10;
	int nline;

	lines = realloc(lines, alloced * sizeof(struct xft_line_t));
	if (lines == NULL)
		err(1, "realloc");

start_wrap:
	nline = 0;
	lastdelim = -1;
	curstr = str;

	for (x = 0; ; x++) {
		struct xft_line_t *line = &lines[nline];
		int tx;

		if (curstr[x] != delim && curstr[x] != '\n' &&
		    curstr[x] != '\0')
			continue;

		XftTextExtentsUtf8(dpy, font, (FcChar8 *)curstr, x, &extents);

		if (curstr[x] == delim && extents.xOff < width) {
			/* keep eating words */
			lastdelim = x;
			continue;
		}

		if (extents.xOff > width) {
			if (lastdelim == -1) {
				/*
				 * We can't break this long line, make
				 * our label width this wide and start
				 * over, since it may affect previous
				 * wrapping
				 */
				width = extents.xOff;
				goto start_wrap;
			}
			x = lastdelim;
		}

		/* trim leading and trailing spaces */
		tx = x;
		while (curstr[tx - 1] == ' ')
			tx--;
		while (curstr[0] == ' ') {
			curstr++;
			tx--;
		}
		XftTextExtentsUtf8(dpy, font, (FcChar8 *)curstr, tx, &extents);

		line->str = curstr;
		line->len = tx;
		line->xft_width = extents.xOff;

		if (curstr[x] == '\0')
			break;

		curstr = curstr + x + 1;
		x = 0;
		lastdelim = -1;
		nline++;

		if (nline == alloced) {
			alloced += 10;
			lines = realloc(lines,
			    alloced * sizeof(struct xft_line_t));
			if (lines == NULL)
				err(1, "realloc");
		}
	}

	*nlines = nline + 1;
	return lines;
}
